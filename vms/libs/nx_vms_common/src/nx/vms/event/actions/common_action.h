// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>

namespace nx {
namespace vms {
namespace event {

/*!
    Initially, this class has been created so that code like "new nx::vms::event::AbstractAction" never appears in project
*/
class NX_VMS_COMMON_API CommonAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit CommonAction(ActionType actionType, const EventParameters& runtimeParams);

    static AbstractActionPtr create(ActionType actionType, const EventParameters& runtimeParams);

    static AbstractActionPtr createBroadcastAction(
        ActionType type,
        const ActionParameters& params);
};

} // namespace event
} // namespace vms
} // namespace nx
