////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "common_action.h"

namespace nx {
namespace vms {
namespace event {

AbstractActionPtr CommonAction::create(
    ActionType actionType,
    const EventParameters& runtimeParams)
{
    return AbstractActionPtr(new CommonAction(actionType, runtimeParams));
}

CommonAction::CommonAction(ActionType actionType, const EventParameters& runtimeParams):
    base_type(actionType, runtimeParams)
{
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
