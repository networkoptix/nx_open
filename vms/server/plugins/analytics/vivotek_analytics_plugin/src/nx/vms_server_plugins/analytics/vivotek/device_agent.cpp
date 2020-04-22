#include "device_agent.h"
#include "nx/utils/thread/cf/cfuture.h"
#include "nx/vms_server_plugins/analytics/vivotek/camera_features.h"

#include <stdexcept>
#include <string>

#define NX_PRINT_PREFIX (m_logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/utils/url.h>

#include "ini.h"
#include "add_ptr_ref.h"
#include "object_types.h"
#include "parse_object_metadata_packet.h"
#include "parameter_api.h"
#include "future_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::utils;
using namespace nx::network;
using namespace nx::network::websocket;

namespace {
    const auto kReopenDelay = 10s;

    const std::string kNewTrackEventType = "nx.vivotek.newTrack";
    const std::string kHelloWorldObjectType = "nx.vivotek.helloWorld";

    std::string makePrintPrefix(const IDeviceInfo* deviceInfo)
    {
        return "[" + libContext().name() + "_device" +
            (!deviceInfo ? "" : (std::string("_") + deviceInfo->id())) + "] ";
    }

    Url getUrl(const IDeviceInfo& deviceInfo)
    {
        Url url = deviceInfo.url();
        url.setUserName(deviceInfo.login());
        url.setPassword(deviceInfo.password());
        url.setPath("");
        return url;
    }

    CameraFeatures fetchFeatures(Url url)
    {
        CameraFeatures features;
        features.fetch(std::move(url)).get();
        return features;
    }
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_basicPollable(std::make_unique<aio::BasicPollable>()),
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT, makePrintPrefix(deviceInfo)),
    m_url(getUrl(*deviceInfo)),
    m_features(fetchFeatures(m_url))
{
    NX_OUTPUT << __func__;

    NX_OUTPUT << "    m_url: " << m_url.toStdString();
}

DeviceAgent::~DeviceAgent()
{
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* /*outResult*/, const IStringMap* rawSettings)
{
    NX_OUTPUT << __func__;

    for (int i = 0; i < rawSettings->count(); ++i)
        NX_OUTPUT << "    " << rawSettings->key(i) << ": " << rawSettings->value(i);

    CameraSettings settings(m_features);
    settings.parse(*rawSettings);

    cf::make_ready_future(cf::unit()).then(*m_basicPollable,
        [&](auto future)
        {
            future.get();

            return settings.store(m_url);
        })
    .get();
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    NX_OUTPUT << __func__;

    CameraSettings settings(m_features);

    cf::make_ready_future(cf::unit()).then(*m_basicPollable,
        [&](auto future)
        {
            future.get();

            return settings.fetch(m_url);
        })
    .get();

    auto rawSettings = makePtr<StringMap>();
    settings.unparse(rawSettings.get());

    for (int i = 0; i < rawSettings->count(); ++i)
        NX_OUTPUT << "    " << rawSettings->key(i) << ": " << rawSettings->value(i);

    auto response = makePtr<SettingsResponse>();
    response->setValues(std::move(rawSettings));

    *outResult = response.releasePtr();
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    NX_OUTPUT << __func__;

    // manifest doesn't depend on setting values, so we don't fetch them
    CameraSettings settings(m_features);

    Json manifest = Json::object{
        {"objectTypes", [&]{
            Json::array types;

            if (m_features.vca)
                types.insert(types.end(), {
                    Json::object{
                        {"id", kObjectTypeHuman},
                        {"name", "Human"},
                    },
                });

            return types;
        }()},
        {"deviceAgentSettingsModel", settings.buildJsonModel()},
    };

    *outResult = new sdk::String(manifest.dump());

    NX_OUTPUT << "    " << outResult->value()->str();
}

void DeviceAgent::setHandler(IHandler* handler)
{
    NX_OUTPUT << __func__;

    cf::make_ready_future(cf::unit()).then(*m_basicPollable,
        [&](auto future)
        {
            future.get();

            m_handler = addPtrRef(handler);

            return cf::unit();
        })
    .get();
}

void DeviceAgent::doSetNeededMetadataTypes(Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
    NX_OUTPUT << __func__;

    NX_OUTPUT << "    eventTypeIds:";
    const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
    for (int i = 0; i < eventTypeIds->count(); ++i)
        NX_OUTPUT << "        " << eventTypeIds->at(i);
    NX_OUTPUT << "    objectTypeIds:";
    const auto objectTypeIds = neededMetadataTypes->objectTypeIds();
    for (int i = 0; i < objectTypeIds->count(); ++i)
        NX_OUTPUT << "        " << objectTypeIds->at(i);

    const bool wantMetadata = !neededMetadataTypes->isEmpty();

    cf::make_ready_future(cf::unit()).then(*m_basicPollable,
        [&](auto future)
        {
            future.get();

            stopMetadataStreaming();

            m_wantMetadata = wantMetadata;
            if (m_wantMetadata)
                return startMetadataStreaming();

            return cf::make_ready_future(cf::unit());
        })
    .get();
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level,
    const QString& caption, const QString& description)
{
    const auto event = makePtr<PluginDiagnosticEvent>(
        level, caption.toStdString(), description.toStdString());

    m_handler->handlePluginDiagnosticEvent(event.get());
}

cf::future<cf::unit> DeviceAgent::startMetadataStreaming()
{
    return m_nativeMetadataSource.open(m_url).then(
        [this, self = addPtrRef(this)](auto future)
        {
            future.get();

            readNextMetadata();

            return cf::unit();
        });
}

void DeviceAgent::stopMetadataStreaming()
{
    m_reopenDelayer.cancelSync();
    m_nativeMetadataSource.close();
}

void DeviceAgent::readNextMetadata()
{
    m_metadataReadRetainer = m_metadataReadRetainer.then(
        [this, self = addPtrRef(this)](auto future)
        {
            try
            {
                try
                {
                    future.get();
                }
                catch (const cf::future_error& exception)
                {
                    if (exception.ecode() == cf::errc::broken_promise)
                        return cf::make_ready_future(cf::unit()); // cancelled
                    throw;
                }
            }
            catch (const std::exception& exception)
            {
                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                    "Metadata streaming failed", exception.what());

                return initiateFuture(
                    [this, self](auto promise)
                    {
                        m_reopenDelayer.start(kReopenDelay,
                            [promise = std::move(promise)]() mutable
                            {
                                promise.set_value(cf::unit());
                            });
                    })
                .then(
                    [this, self](auto future)
                    {
                        future.get();

                        return startMetadataStreaming();
                    });
            }

            return m_nativeMetadataSource.read().then(
                [this, self = std::move(self)](auto future)
                {
                    const auto nativeMetadata = future.get();

                    if (auto packet = parseObjectMetadataPacket(nativeMetadata))
                        m_handler->handleMetadata(packet.releasePtr());

                    readNextMetadata();

                    return cf::unit();
                });
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
