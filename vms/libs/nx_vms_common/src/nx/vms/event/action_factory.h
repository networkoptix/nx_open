// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/rule.h>

namespace nx::vms::common { class SystemContext; }

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ActionFactory
{
public:
    static AbstractActionPtr instantiateAction(
        nx::vms::common::SystemContext* systemContext,
        const RulePtr& rule,
        const AbstractEventPtr& event,
        const QnUuid& moduleGuid,
        EventState state = EventState::undefined);

    static AbstractActionPtr instantiateAction(
        nx::vms::common::SystemContext* systemContext,
        const RulePtr& rule,
        const AbstractEventPtr& event,
        const QnUuid& moduleGuid,
        const AggregationInfo& aggregationInfo);

    static AbstractActionPtr createAction(const ActionType actionType,
        const EventParameters& runtimeParams);

    static AbstractActionPtr cloneAction(AbstractActionPtr action);
};

} // namespace event
} // namespace vms
} // namespace nx
