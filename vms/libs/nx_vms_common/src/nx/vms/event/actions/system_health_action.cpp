// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_action.h"

#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/helpers.h>
#include <nx_ec/data/api_conversion_functions.h>
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
    runtimeParams.eventType = EventType(EventType::siteHealthEvent + (int) message);
    runtimeParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    runtimeParams.attributes = attributes;
    setRuntimeParams(runtimeParams);

    ActionParameters actionParams;
    actionParams.additionalResources =
        {api::kAllPowerUserGroupIds.begin(), api::kAllPowerUserGroupIds.end()};
    setParams(actionParams);
}

AbstractActionPtr SystemHealthAction::fromServerRuntimeEvent(
    const nx::vms::api::ServerRuntimeEventData& data)
{
    using namespace nx::vms::api;

    if (data.eventType != ServerRuntimeEventType::systemHealthMessage)
        return {};

    const auto [actionData, result] = nx::reflect::json::deserialize<EventActionData>(
        nx::toBufferView(data.eventData));
    if (!NX_ASSERT(result, "Can't deserialize system health message: %1", result.toString()))
        return {};

    AbstractActionPtr action;
    ec2::fromApiToResource(actionData, action);

    if (action && action->actionType() == ActionType::showPopupAction
        && isSiteHealth(action->eventType()))
    {
        return action;
    }

    return {};
}

} // namespace nx::vms::event
