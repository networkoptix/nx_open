#include "device_agent.h"
#include "nx/vms_server_plugins/analytics/vivotek/camera_features.h"

#include <exception>

#define NX_PRINT_PREFIX (m_logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/utils/url.h>

#include "ini.h"
#include "camera_settings.h"
#include "object_types.h"
#include "parse_object_metadata_packet.h"
#include "exception_utils.h"
#include "json_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::utils;
using namespace nx::network;

namespace {

void emitDiagnostic(IDeviceAgent::IHandler* handler,
    IPluginDiagnosticEvent::Level level, const QString& caption, const QString& description)
{
    const auto event = makePtr<PluginDiagnosticEvent>(level, caption.toStdString(), description.toStdString());
    handler->handlePluginDiagnosticEvent(event.get());
}

} // namespace

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_basicPollable(std::make_unique<aio::BasicPollable>()),
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT,
        NX_FMT("[%1_device_%2]", libContext().name(), deviceInfo->id()).toStdString()),
    m_url(
        [&]
        {
            Url url = deviceInfo->url();
            url.setUserName(deviceInfo->login());
            url.setPassword(deviceInfo->password());
            url.setPath("");
            return url;
        }()),
    m_features(CameraFeatures::fetchFrom(m_url))
{
}

DeviceAgent::~DeviceAgent()
{
    if (m_wantMetadata)
        stopMetadataStreaming();
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* outResult, const IStringMap* values)
{
    try
    {
        CameraSettings settings(m_features);
        settings.parseFromServer(*values);
        settings.storeTo(m_url);
        *outResult = settings.getErrorMessages().releasePtr();
    }
    catch (const std::exception& exception)
    {
        *outResult = toSdkError(exception);
    }
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    try
    {
        CameraSettings settings(m_features);
        settings.fetchFrom(m_url);
        auto values = settings.unparseToServer();
        auto errorMessages = settings.getErrorMessages();
        *outResult = new SettingsResponse(std::move(values), std::move(errorMessages));
    }
    catch (const std::exception& exception)
    {
        *outResult = toSdkError(exception);
    }
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    try
    {
        auto manifest = QJsonObject{
            {"objectTypes",
                [&]{
                    QJsonArray types;

                    if (m_features.vca)
                    {
                        types.push_back(QJsonObject{
                            {"id", kObjectTypeHuman},
                            {"name", "Human"},
                        });
                    }

                    return types;
                }(),
            },
            {"deviceAgentSettingsModel", CameraSettings::getModelForManifest(m_features)},
        };

        *outResult = new sdk::String(unparseJson(manifest).toStdString());
    }
    catch (const std::exception& exception)
    {
        *outResult = toSdkError(exception);
    }
}

void DeviceAgent::setHandler(IHandler* handler)
{
    try
    {
        m_basicPollable->executeInAioThreadSync(
            [&]{
                handler->addRef();
                m_handler = toPtr(handler);
            });
    }
    catch (const std::exception& exception)
    {
        vivotek::emitDiagnostic(handler, IPluginDiagnosticEvent::Level::error,
            "Failed to set device agent handler", collectNestedMessages(exception));
    }
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    try
    {
        const bool wantMetadata = !neededMetadataTypes->isEmpty();
        if (!wantMetadata && m_wantMetadata)
        {
            m_wantMetadata = wantMetadata;
            m_basicPollable->executeInAioThreadSync([&]{ stopMetadataStreaming(); });
        }
        else if (wantMetadata && !m_wantMetadata)
        {
            m_wantMetadata = wantMetadata;
            cf::initiate(*m_basicPollable,
                [&]
                {
                    return startMetadataStreaming();
                })
                .get();
        }
    }
    catch (const std::exception& exception)
    {
        *outResult = toSdkError(exception);
    }
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level, const QString& caption, const QString& description)
{
    vivotek::emitDiagnostic(m_handler.get(),
        level, std::move(caption), std::move(description));
}

cf::future<cf::unit> DeviceAgent::startMetadataStreaming()
{
    m_nativeMetadataSource.emplace();
    return m_nativeMetadataSource->open(m_url)
        .then_ok([this](auto&&) { return streamMetadataPackets(); });
}

cf::future<cf::unit> DeviceAgent::restartMetadataStreamingLater()
{
    constexpr auto kRestartDelay = 10s;

    m_restartDelayer.emplace();
    return m_restartDelayer->wait(kRestartDelay)
        .then_ok([this](auto&&) { return startMetadataStreaming(); })
        .then_fail(
            [this](const std::exception& exception)
            {
                if (nestedContains(exception, std::errc::operation_canceled))
                    return cf::make_ready_future(cf::unit());

                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                    "Failed to restart metadata streaming", collectNestedMessages(exception));

                return restartMetadataStreamingLater();
            });
}

void DeviceAgent::stopMetadataStreaming()
{
    m_restartDelayer.reset();
    m_nativeMetadataSource.reset();
}

cf::future<cf::unit> DeviceAgent::streamMetadataPackets()
{
    return m_nativeMetadataSource->read()
        .then_ok(
            [this](const auto& nativePacket)
            {
                if (auto packet = parseObjectMetadataPacket(nativePacket))
                    m_handler->handleMetadata(packet.releasePtr());

                streamMetadataPackets()
                    .then_fail(
                        [this](const std::exception& exception)
                        {
                            emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                                "Metadata streaming failed", collectNestedMessages(exception));

                            return restartMetadataStreamingLater();
                        });

                return cf::unit();
            })
        .then_fail(
            [](const std::exception& exception)
            {
                if (nestedContains(exception, std::errc::operation_canceled))
                    return cf::unit();

                throw;
            });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
