#include "instant_event.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace event {

InstantEvent::InstantEvent(EventType eventType, const QnResourcePtr& resource, qint64 timeStamp):
    base_type(eventType, resource, EventState::undefined, timeStamp)
{
}

bool InstantEvent::isEventStateMatched(EventState state, ActionType /*actionType*/) const
{
    // Rule MUST contain 'Not Defined' event state filter
    return state == EventState::undefined;
}

} // namespace event
} // namespace vms
} // namespace nx
