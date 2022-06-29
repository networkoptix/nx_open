// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    const QnUuid& eventResourceId,
    const nx::common::metadata::Attributes& attributes)
    :
    base_type(ActionType::showPopupAction, EventParameters())
{
    EventParameters runtimeParams;
    runtimeParams.eventType = EventType(EventType::systemHealthEvent + message);
    runtimeParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    runtimeParams.attributes = attributes;
    setRuntimeParams(runtimeParams);

    ActionParameters actionParams;
    const auto& ids = QnPredefinedUserRoles::adminIds();
    actionParams.additionalResources = std::vector(ids.begin(), ids.end());
    setParams(actionParams);
}

} // namespace event
} // namespace vms
} // namespace nx
