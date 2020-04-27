#include "server_runtime_event_connector.h"

#include <functional>

#include <client/desktop_client_message_processor.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

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
            DeviceAgentSettingsMaybeChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit deviceAgentSettingsMaybeChanged(
                result.deviceId,
                result.engineId);

            return;
        }
        case ServerRuntimeEventType::deviceFootageChanged:
        {
            DeviceFootageChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit deviceFootageChanged(result.deviceIds);

            return;
        }
        default:
            NX_ASSERT(false, "Unexpected event type %1", eventData.eventType);
    }
}

} // namespace nx::vms::client::desktop
