#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx {
namespace vms {
namespace event {

class StorageFailureEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    StorageFailureEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& storageUrl);
};

} // namespace event
} // namespace vms
} // namespace nx
