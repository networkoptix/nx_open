#include "reasoned_event.h"

namespace nx {
namespace vms {
namespace event {

ReasonedEvent::ReasonedEvent(
    EventType eventType,
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonParamsEncoded)
    :
    base_type(eventType, resource, timeStamp),
    m_reasonCode(reasonCode),
    m_reasonParamsEncoded(reasonParamsEncoded)
{
}

EventParameters ReasonedEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.reasonCode = m_reasonCode;
    params.description = m_reasonParamsEncoded;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
