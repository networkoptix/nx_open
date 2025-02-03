// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
