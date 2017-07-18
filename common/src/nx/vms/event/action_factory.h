#pragma once

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/rule.h>

namespace nx {
namespace vms {
namespace event {

class ActionFactory
{
public:
    static AbstractActionPtr instantiateAction(
        const RulePtr& rule,
        const AbstractEventPtr& event,
        const QnUuid& moduleGuid,
        EventState state = EventState::undefined);

    static AbstractActionPtr instantiateAction(
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
