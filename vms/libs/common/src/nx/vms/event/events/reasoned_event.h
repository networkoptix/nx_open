#pragma once

#include <nx/vms/event/events/instant_event.h>

namespace nx {
namespace vms {
namespace event {

class ReasonedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit ReasonedEvent(EventType eventType, const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonParamsEncoded);

    virtual EventParameters getRuntimeParams() const override;

protected:
    const EventReason m_reasonCode;
    const QString m_reasonParamsEncoded;
};

} // namespace event
} // namespace vms
} // namespace nx
