// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_action.h"

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <utils/common/synctime.h>

namespace nx::vms::event {

SystemHealthAction::SystemHealthAction(
    common::system_health::MessageType message,
    nx::Uuid eventResourceId,
    const nx::common::metadata::Attributes& attributes)
    :
    base_type(ActionType::showPopupAction, EventParameters())
{
    EventParameters runtimeParams;
    runtimeParams.eventType = EventType(EventType::systemHealthEvent + (int) message);
    runtimeParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    runtimeParams.attributes = attributes;
    setRuntimeParams(runtimeParams);

    ActionParameters actionParams;
    actionParams.additionalResources = {api::kAllPowerUserGroupIds.begin(), api::kAllPowerUserGroupIds.end()};
    setParams(actionParams);
}

} // namespace nx::vms::event
