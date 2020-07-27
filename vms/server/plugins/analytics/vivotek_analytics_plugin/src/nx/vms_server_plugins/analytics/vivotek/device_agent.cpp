#include "device_agent.h"

#include <system_error>

#define NX_PRINT_PREFIX (m_logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/utils/url.h>
#include <nx/network/url/url_builder.h>

#include "ini.h"
#include "object_types.h"
#include "event_types.h"
#include "parse_object_metadata_packet.h"
#include "parse_event_metadata_packets.h"
#include "exception.h"
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

constexpr auto kMetadataStreamingRestartDelay = 10s;

void emitDiagnostic(IDeviceAgent::IHandler* handler,
    IPluginDiagnosticEvent::Level level, const QString& caption, const QString& description)
{
    const auto event = makePtr<PluginDiagnosticEvent>(level, caption.toStdString(), description.toStdString());
    handler->handlePluginDiagnosticEvent(event.get());
}

const auto ignoreCancellation =
    [](const Exception& exception)
    {
        if (exception.errorCode() != std::errc::operation_canceled &&
            exception.errorCode() != std::errc::connection_aborted)
        {
            throw;
        }

        return cf::unit();
    };

} // namespace

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_basicPollable(std::make_unique<aio::BasicPollable>()),
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT,
        NX_FMT("[%1_device_%2]", libContext().name(), deviceInfo->id()).toStdString()),
    m_url(network::url::Builder(deviceInfo->url())
        .setUserName(deviceInfo->login())
        .setPassword(deviceInfo->password())
        .setPath("")
        .toUrl())
{
}

void DeviceAgent::doSetSettings(Result<const ISettingsResponse*>* outResult, const IStringMap* values)
{
    try
    {
        CameraSettings settings;

        settings.parseFromServer(*values);

        settings.storeTo(m_url);
        settings.fetchFrom(m_url);

        *outResult = settings.serializeToServer().releasePtr();
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(ErrorCode::internalError, exception.what());
    }
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    try
    {
        CameraSettings settings;

        settings.fetchFrom(m_url);

        auto& self = const_cast<DeviceAgent&>(*this);
        self.updateAvailableMetadataTypes(settings);
        self.refreshMetadataStreaming();

        *outResult = settings.serializeToServer().releasePtr();
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(ErrorCode::internalError, exception.what());
    }
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    try
    {
        auto manifest = QJsonObject{
            {"objectTypes",
                [&]()
                {
                    QJsonArray types;

                    for (const auto& objectType: kObjectTypes)
                    {
                        types.push_back(QJsonObject{
                            {"id", objectType.id},
                            {"name", objectType.prettyName},
                        });
                    }

                    return types;
                }(),
            },
            {"eventTypes",
                [&]()
                {
                    QJsonArray types;

                    for (const auto& eventType: kEventTypes)
                    {
                        QJsonObject type = {
                            {"id", eventType.id},
                            {"name", eventType.prettyName},
                        };
                        if (eventType.isProlonged)
                            type["flags"] = "stateDependent";

                        types.push_back(std::move(type));
                    }

                    return types;
                }(),
            },
        };

        *outResult = new sdk::String(serializeJson(manifest).toStdString());
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(ErrorCode::internalError, exception.what());
    }
}

void DeviceAgent::setHandler(IHandler* handler)
{
    try
    {
        m_basicPollable->executeInAioThreadSync(
            [&]()
            {
                handler->addRef();
                m_handler = toPtr(handler);
            });
    }
    catch (const std::exception& exception)
    {
        vivotek::emitDiagnostic(handler, IPluginDiagnosticEvent::Level::error,
            "Failed to set device agent handler", exception.what());
    }
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    try
    {
        m_neededMetadataTypes = NoNativeMetadataTypes;

        m_neededMetadataTypes.setFlag(
            ObjectNativeMetadataType, !!neededMetadataTypes->objectTypeIds()->count());
        m_neededMetadataTypes.setFlag(
            EventNativeMetadataType, !!neededMetadataTypes->eventTypeIds()->count());

        refreshMetadataStreaming();
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(ErrorCode::internalError, exception.what());
    }
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level, const QString& caption, const QString& description)
{
    vivotek::emitDiagnostic(m_handler.get(),
        level, std::move(caption), std::move(description));
}

void DeviceAgent::updateAvailableMetadataTypes(const CameraSettings& settings)
{
    m_availableMetadataTypes = NoNativeMetadataTypes;

    for (const auto& objectType: kObjectTypes)
    {
        if (objectType.isAvailable(settings))
        {
            m_availableMetadataTypes |= ObjectNativeMetadataType;
            break;
        }
    }

    for (auto const& eventType: kEventTypes)
    {
        if (eventType.isAvailable(settings))
        {
            m_availableMetadataTypes |= EventNativeMetadataType;
            break;
        }
    }
}

void DeviceAgent::refreshMetadataStreaming()
{
    const auto shouldBeStreamed = m_availableMetadataTypes & m_neededMetadataTypes;
    if (shouldBeStreamed != m_streamedMetadataTypes)
    {
        m_basicPollable->executeInAioThreadSync([&]{ stopMetadataStreaming(); });

        m_streamedMetadataTypes = shouldBeStreamed;

        if (m_streamedMetadataTypes != NoNativeMetadataTypes)
            m_basicPollable->executeInAioThreadSync([&]{ startMetadataStreaming(); });
    }
}

void DeviceAgent::startMetadataStreaming()
{
    m_nativeMetadataSource.emplace();
    m_nativeMetadataSource->open(m_url, m_streamedMetadataTypes)
        .then_unwrap(
            [this](auto&&) {
                streamMetadataPackets();

                return cf::unit();
            })
        .catch_(ignoreCancellation)
        .catch_(
            [this](const std::exception& exception)
            {
                emitDiagnostic(IPluginDiagnosticEvent::Level::warning,
                    "Failed to start metadata streaming", exception.what());

                m_timer.emplace();
                return m_timer->start(kMetadataStreamingRestartDelay)
                    .then_unwrap(
                        [this](auto&&) {
                            startMetadataStreaming();
                            return cf::unit();
                        })
                    .catch_(ignoreCancellation);
            });
}

void DeviceAgent::stopMetadataStreaming()
{
    m_timer.reset();
    m_nativeMetadataSource.reset();
}

void DeviceAgent::streamMetadataPackets()
{
    m_nativeMetadataSource->read()
        .then_unwrap(
            [this](const auto& nativePacket)
            {
                if (auto packet = parseObjectMetadataPacket(nativePacket))
                    m_handler->handleMetadata(packet.releasePtr());
                for (auto& packet: parseEventMetadataPackets(nativePacket))
                    m_handler->handleMetadata(packet.releasePtr());

                streamMetadataPackets();

                return cf::unit();
            })
        .catch_(ignoreCancellation)
        .catch_(
            [this](const std::exception& exception)
            {
                emitDiagnostic(IPluginDiagnosticEvent::Level::warning,
                    "Metadata streaming failed", exception.what());

                startMetadataStreaming();

                return cf::unit();
            });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
