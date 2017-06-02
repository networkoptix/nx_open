#include "system_health_action.h"

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/action_parameters.h>

#include <utils/common/synctime.h>

namespace nx {
namespace vms {
namespace event {

SystemHealthAction::SystemHealthAction(
    QnSystemHealth::MessageType message,
    const QnUuid& eventResourceId)
    :
    base_type(ShowPopupAction, EventParameters())
{
    EventParameters runtimeParams;
    runtimeParams.eventType = EventType(SystemHealthEvent + message);
    runtimeParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    setRuntimeParams(runtimeParams);

    ActionParameters actionParams;
    actionParams.userGroup = AdminOnly;
    setParams(actionParams);
}

} // namespace event
} // namespace vms
} // namespace nx
