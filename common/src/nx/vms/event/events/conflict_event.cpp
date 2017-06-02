#include "conflict_event.h"

namespace nx {
namespace vms {
namespace event {

ConflictEvent::ConflictEvent(
    const EventType eventType,
    const QnResourcePtr& resource,
    const qint64 timeStamp,
    const QString& caption,
    const QString& description)
    :
    base_type(eventType, resource, timeStamp),
    m_caption(caption),
    m_description(description)
{
}

EventParameters ConflictEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.caption = m_caption;
    params.description = m_description;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
