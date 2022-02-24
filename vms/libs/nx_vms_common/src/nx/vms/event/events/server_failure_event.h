// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ServerFailureEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    explicit ServerFailureEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonText);
};

} // namespace event
} // namespace vms
} // namespace nx
