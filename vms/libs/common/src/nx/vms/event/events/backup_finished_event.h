#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx {
namespace vms {
namespace event {

class BackupFinishedEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    explicit BackupFinishedEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonText);
};

} // namespace event
} // namespace vms
} // namespace nx
