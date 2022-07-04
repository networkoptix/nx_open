// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/instant_event.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ReasonedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit ReasonedEvent(EventType eventType, const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonParamsEncoded);

    virtual EventParameters getRuntimeParams() const override;

    EventReason getReasonCode() const;
    const QString& getReasonText() const;

protected:
    const EventReason m_reasonCode;
    const QString m_reasonParamsEncoded;
};

} // namespace event
} // namespace vms
} // namespace nx
