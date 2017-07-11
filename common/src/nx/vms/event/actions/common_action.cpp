////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "common_action.h"

namespace nx {
namespace vms {
namespace event {

CommonAction::CommonAction(ActionType actionType, const EventParameters& runtimeParams):
    base_type(actionType, runtimeParams)
{
}

AbstractActionPtr CommonAction::createCopy(
    ActionType actionType,
    const AbstractActionPtr& source)
{
    AbstractActionPtr result(new CommonAction(actionType, source->getRuntimeParams()));

    result->setResources(source->getResources());
    result->setParams(source->getParams());
    result->setRuleId(source->getRuleId());
    result->setToggleState(source->getToggleState());
    result->setReceivedFromRemoteHost(source->isReceivedFromRemoteHost());
    result->setAggregationCount(source->getAggregationCount());

    return result;
}

AbstractActionPtr CommonAction::createBroadcastAction(
    ActionType type,
    const ActionParameters& params)
{
    AbstractActionPtr result(new CommonAction(type, EventParameters()));
    result->setParams(params);
    return result;
}

} // namespace event
} // namespace vms
} // namespace nx
