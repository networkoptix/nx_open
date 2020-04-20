#include "device_agent.h"
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
#include "async_utils.h"

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
    waitHandler(
        [&](auto handler) {
            features.fetch(std::move(url), std::move(handler));
        });
    return features;
}

} // namespace

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_basicPollable(std::make_unique<aio::BasicPollable>()),
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT, makePrintPrefix(deviceInfo)),
    m_url(getUrl(*deviceInfo)),
    m_features(fetchFeatures(m_url))
{
    NX_OUTPUT << __func__;

    NX_OUTPUT << "    m_url: " << m_url.toStdString();
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* /*outResult*/, const IStringMap* rawSettings)
{
    NX_OUTPUT << __func__;

    m_basicPollable->executeInAioThreadSync(
        [&]{
            for (int i = 0; i < rawSettings->count(); ++i)
                NX_OUTPUT << "    " << rawSettings->key(i) << ": " << rawSettings->value(i);

            CameraSettings settings(m_features);
            settings.parse(*rawSettings);
            waitHandler(
                [&](auto&& handler) mutable
                {
                    settings.store(m_url, std::forward<decltype(handler)>(handler));
                });
        });
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    NX_OUTPUT << __func__;

    m_basicPollable->executeInAioThreadSync(
        [&]{
            CameraSettings settings(m_features);
            waitHandler(
                [&](auto&& handler) mutable
                {
                    settings.fetch(m_url, std::forward<decltype(handler)>(handler));
                });

            auto rawSettings = makePtr<StringMap>();
            settings.unparse(rawSettings.get());

            for (int i = 0; i < rawSettings->count(); ++i)
                NX_OUTPUT << "    " << rawSettings->key(i) << ": " << rawSettings->value(i);

            auto response = makePtr<SettingsResponse>();
            response->setValues(std::move(rawSettings));

            *outResult = response.releasePtr();
        });
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    NX_OUTPUT << __func__;

    m_basicPollable->executeInAioThreadSync(
        [&]{
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
                {"deviceAgentSettingsModel", settings.buildModel()},
            };

            *outResult = new sdk::String(manifest.dump());

            NX_OUTPUT << "    " << outResult->value()->str();
        });
}

void DeviceAgent::setHandler(IHandler* handler)
{
    NX_OUTPUT << __func__;

    m_basicPollable->executeInAioThreadSync(
        [&]{
            m_handler = addPtrRef(handler);
        });
}

void DeviceAgent::doSetNeededMetadataTypes(Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
    NX_OUTPUT << __func__;

    m_basicPollable->executeInAioThreadSync(
        [&]{
            NX_OUTPUT << "    eventTypeIds:";
            const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
            for (int i = 0; i < eventTypeIds->count(); ++i)
                NX_OUTPUT << "        " << eventTypeIds->at(i);
            NX_OUTPUT << "    objectTypeIds:";
            const auto objectTypeIds = neededMetadataTypes->objectTypeIds();
            for (int i = 0; i < objectTypeIds->count(); ++i)
                NX_OUTPUT << "        " << objectTypeIds->at(i);

            const bool wantMetadata = !neededMetadataTypes->isEmpty();
            if (m_wantMetadata == wantMetadata)
                return;

            m_wantMetadata = wantMetadata;

            reopenNativeMetadataSource();
        });
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level,
    const QString& caption, const QString& description)
{
    const auto event = makePtr<PluginDiagnosticEvent>(
        level, caption.toStdString(), description.toStdString());

    m_handler->handlePluginDiagnosticEvent(event.get());
}

void DeviceAgent::reopenNativeMetadataSource()
{
    NX_OUTPUT << __func__;

    m_reopenDelayer.cancelAsync(
        [this, self = addPtrRef(this)]() mutable
        {
            NX_OUTPUT << "    reopenDelayer cancelled";

            m_nativeMetadataSource.close(
                [this, self = std::move(self)]() mutable
                {
                    NX_OUTPUT << "    nativeMetadataSource closed";

                    if (!m_wantMetadata)
                        return;

                    m_nativeMetadataSource.open(m_url,
                        [this, self = std::move(self)](auto exceptionPtr) mutable
                        {
                            try
                            {
                                if (exceptionPtr)
                                    std::rethrow_exception(exceptionPtr);

                                NX_OUTPUT << "    nativeMetadataSource opened";

                                readNextMetadata();
                            }
                            catch (std::exception const& exception)
                            {
                                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                                    "Failed to open metadata source", exception.what());

                                m_reopenDelayer.start(kReopenDelay,
                                    [this, self = std::move(self)]()
                                    {
                                        reopenNativeMetadataSource();
                                    });
                            }
                        });
                });
        });
}

void DeviceAgent::readNextMetadata()
{
    m_nativeMetadataSource.read(
        [this, self = addPtrRef(this)](auto exceptionPtr, Json nativeMetadata) mutable
        {
            try
            {
                if (exceptionPtr)
                    std::rethrow_exception(exceptionPtr);

                if (auto packet = parseObjectMetadataPacket(nativeMetadata))
                    m_handler->handleMetadata(packet.releasePtr());

                readNextMetadata();
            }
            catch (std::exception const& exception)
            {
                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                    "Failed to read metadata", exception.what());

                reopenNativeMetadataSource();
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
