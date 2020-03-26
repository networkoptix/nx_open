#include "server_runtime_event_connector.h"

#include <functional>

#include <client/desktop_client_message_processor.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

template<typename EventData, typename EventDataHandler>
void handleEvent(
    const nx::utils::log::Tag& logTag,
    const ServerRuntimeEventData& eventData,
    EventDataHandler handler)
{
    bool success = false;
    const EventData deserialized = QJson::deserialized(eventData.eventData, EventData(), &success);

    if (!success)
    {
        NX_DEBUG(logTag,
            "Unable to deserialize a server runtime event. Event type: %1, event data: %2",
            eventData.eventType, eventData.eventData);

        return;
    }

    handler(deserialized);
}

ServerRuntimeEventConnector::ServerRuntimeEventConnector(
    QnDesktopClientMessageProcessor* messageProcessor)
{
    connect(
        messageProcessor,
        &QnDesktopClientMessageProcessor::serverRuntimeEventOccurred,
        this,
        &ServerRuntimeEventConnector::at_serverRuntimeEventOccurred);
}


void ServerRuntimeEventConnector::at_serverRuntimeEventOccurred(
    const ServerRuntimeEventData& eventData)
{
    static const nx::utils::log::Tag kLogTag(typeid(*this));

    switch (eventData.eventType)
    {
        case ServerRuntimeEventType::deviceAgentSettingsMaybeChanged:
        {
            handleEvent<DeviceAgentSettingsMaybeChangedData>(
                kLogTag,
                eventData,
                [this](const DeviceAgentSettingsMaybeChangedData& deserialized)
                {
                    emit deviceAgentSettingsMaybeChanged(
                        deserialized.deviceId,
                        deserialized.engineId);
                });

            return;
        }
        case ServerRuntimeEventType::deviceFootageChanged:
        {
            handleEvent<DeviceFootageChangedData>(
                kLogTag,
                eventData,
                [this](const DeviceFootageChangedData& deserialized)
                {
                    emit deviceFootageChanged(deserialized.deviceIds);
                });

            return;
        }
        default:
            NX_ASSERT(false, "Unexpected event type %1", eventData.eventType);
    }
}

} // namespace nx::vms::client::desktop
