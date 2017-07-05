#include "system_health_action.h"

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/action_parameters.h>

#include <core/resource_management/user_roles_manager.h>
#include <utils/common/synctime.h>

namespace nx {
namespace vms {
namespace event {

SystemHealthAction::SystemHealthAction(
    QnSystemHealth::MessageType message,
    const QnUuid& eventResourceId)
    :
    base_type(showPopupAction, EventParameters())
{
    EventParameters runtimeParams;
    runtimeParams.eventType = EventType(systemHealthEvent + message);
    runtimeParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    setRuntimeParams(runtimeParams);

    ActionParameters actionParams;
    actionParams.additionalResources = QnUserRolesManager::adminRoleIds().toVector().toStdVector();
    setParams(actionParams);
}

} // namespace event
} // namespace vms
} // namespace nx
