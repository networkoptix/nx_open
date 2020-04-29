#include "server_runtime_event_manager.h"

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>
#include <transaction/message_bus_adapter.h>
#include <media_server/media_server_module.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::server::event {

using namespace nx::vms::api;

template<typename EventData>
ServerRuntimeEventData makeEvent(ServerRuntimeEventType eventType, const EventData& eventData)
{
    ServerRuntimeEventData result;
    result.eventType = eventType;
    result.eventData = QJson::serialized(eventData);

    return result;
}

ServerRuntimeEventManager::ServerRuntimeEventManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

void ServerRuntimeEventManager::triggerDeviceAgentSettingsMaybeChangedEvent(
    QnUuid deviceId,
    QnUuid engineId)
{
    const DeviceAgentSettingsMaybeChangedData eventData{deviceId, engineId};
    sendEvent(makeEvent(ServerRuntimeEventType::deviceAgentSettingsMaybeChanged, eventData));
}

void ServerRuntimeEventManager::triggerDeviceFootageChangedEvent(std::vector<QnUuid> deviceIds)
{
    const DeviceFootageChangedData eventData{std::move(deviceIds)};
    sendEvent(makeEvent(ServerRuntimeEventType::deviceFootageChanged, eventData));
}

void ServerRuntimeEventManager::triggerAnalyticsStorageParametersChanged(QnUuid serverId)
{
    const AnalyticsStorageParametersChangedData eventData{serverId};
    sendEvent(makeEvent(ServerRuntimeEventType::analyticsStorageParametersChanged, eventData));
}

void ServerRuntimeEventManager::sendEvent(const ServerRuntimeEventData& eventData)
{
    ec2::TransactionMessageBusAdapter* const messageBus =
        serverModule()->ec2Connection()->messageBus();

    ec2::QnTransaction<ServerRuntimeEventData> transaction(
        ec2::ApiCommand::serverRuntimeEvent,
        serverModule()->commonModule()->moduleGUID(),
        eventData);

    messageBus->sendTransaction(transaction);
}

} // namespace nx::vms::server::event
