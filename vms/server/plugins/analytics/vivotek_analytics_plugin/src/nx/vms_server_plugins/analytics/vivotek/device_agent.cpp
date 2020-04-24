#include "device_agent.h"

#include <exception>
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
#include "exception.h"
#include "add_ptr_ref.h"
#include "object_types.h"
#include "parse_object_metadata_packet.h"
#include "parameter_api.h"

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

    void emitDiagnostic(IDeviceAgent::IHandler* handler,
        IPluginDiagnosticEvent::Level level, std::string caption, std::string description)
    {
        const auto event = makePtr<PluginDiagnosticEvent>(level, caption, description);
        handler->handlePluginDiagnosticEvent(event.get());
    }
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_basicPollable(std::make_unique<aio::BasicPollable>()),
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT, makePrintPrefix(deviceInfo)),
    m_url(getUrl(*deviceInfo)),
    m_features(fetchFeatures(m_url))
{
    NX_OUTPUT << __func__;

    m_nativeMetadataSource.bindToAioThread(m_basicPollable->getAioThread());
    m_reopenDelayer.bindToAioThread(m_basicPollable->getAioThread());

    NX_OUTPUT << "    m_url: " << m_url.toStdString();
}

DeviceAgent::~DeviceAgent()
{
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* outResult, const IStringMap* rawSettings)
{
    try
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
    catch (...)
    {
        *outResult = toError(std::current_exception());
    }
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    try
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
        settings.unparse(rawSettings.get()); // TODO: implement per-setting errors

        for (int i = 0; i < rawSettings->count(); ++i)
            NX_OUTPUT << "    " << rawSettings->key(i) << ": " << rawSettings->value(i);

        auto response = makePtr<SettingsResponse>();
        response->setValues(std::move(rawSettings));

        *outResult = response.releasePtr();
    }
    catch (...)
    {
        *outResult = toError(std::current_exception());
    }
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    try
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
    catch (...)
    {
        *outResult = toError(std::current_exception());
    }
}

void DeviceAgent::setHandler(IHandler* handler)
{
    try
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
    catch (...)
    {
        vivotek::emitDiagnostic(handler,
            IPluginDiagnosticEvent::Level::error,
            "Failed to set device agent handler",
            toError(std::current_exception()).errorMessage()->str());
    }
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    try
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
    catch (...)
    {
        *outResult = toError(std::current_exception());
    }
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level,
    std::string caption, std::string description)
{
    vivotek::emitDiagnostic(m_handler.get(),
        level, std::move(caption), std::move(description));
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
    m_reopenDelayer.cancel();
    m_nativeMetadataSource.close();
}

void DeviceAgent::readNextMetadata()
{
    m_metadataReadRetainer = m_metadataReadRetainer.then(
        [this, self = addPtrRef(this)](auto future)
        {
            try
            {
                future.get();
            }
            catch (...)
            {
                if (isCancelled(std::current_exception()))
                    return cf::make_ready_future(cf::unit());

                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                    "Metadata streaming failed",
                    toError(std::current_exception()).errorMessage()->str());

                return m_reopenDelayer.wait(kReopenDelay).then(
                    [this, self = std::move(self)](auto future)
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
