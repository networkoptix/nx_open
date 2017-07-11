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
class CommonAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit CommonAction(ActionType actionType, const EventParameters& runtimeParams);

    static AbstractActionPtr createCopy(
        ActionType actionType,
        const AbstractActionPtr& source);

    static AbstractActionPtr createBroadcastAction(
        ActionType type,
        const ActionParameters& params);
};

} // namespace event
} // namespace vms
} // namespace nx
