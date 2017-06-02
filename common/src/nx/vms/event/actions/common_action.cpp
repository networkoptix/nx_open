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

} // namespace event
} // namespace vms
} // namespace nx
