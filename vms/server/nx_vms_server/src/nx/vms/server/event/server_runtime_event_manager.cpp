#include "server_runtime_event_manager.h"

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>
#include <transaction/message_bus_adapter.h>
#include <media_server/media_server_module.h>
#include <nx/vms/api/data/server_runtime_event_data.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::server::event {

ServerRuntimeEventManager::ServerRuntimeEventManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

void ServerRuntimeEventManager::triggerDeviceAgentSettingsMaybeChangedEvent(
    QnUuid deviceId,
    QnUuid engineId)
{
    ec2::TransactionMessageBusAdapter* const messageBus =
        serverModule()->ec2Connection()->messageBus();

    const nx::vms::api::DeviceAgentSettingsMaybeChangedData eventData{
        deviceId,
        engineId};

    nx::vms::api::ServerRuntimeEventData event;
    event.eventType = nx::vms::api::ServerRuntimeEventType::deviceAgentSettingsMaybeChanged;
    event.eventData = QJson::serialized(eventData);

    ec2::QnTransaction<nx::vms::api::ServerRuntimeEventData> transaction(
        ec2::ApiCommand::serverRuntimeEvent,
        serverModule()->commonModule()->moduleGUID(),
        event);

    messageBus->sendTransaction(transaction);
}

} // namespace nx::vms::server::event
