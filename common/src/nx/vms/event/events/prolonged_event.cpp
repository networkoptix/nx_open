#include "prolonged_event.h"

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace event {

ProlongedEvent::ProlongedEvent(
    EventType eventType,
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStamp)
    :
    base_type(eventType, resource, toggleState, timeStamp)
{
    NX_ASSERT(hasToggleState(eventType));
}

bool ProlongedEvent::isEventStateMatched(EventState state, ActionType actionType) const
{
    return state == EventState::undefined
        || state == getToggleState()
        || hasToggleState(actionType);
}

} // namespace event
} // namespace vms
} // namespace nx
