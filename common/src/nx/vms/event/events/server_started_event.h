#pragma once

#include <nx/vms/event/events/instant_event.h>

namespace nx {
namespace vms {
namespace event {

class ServerStartedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    ServerStartedEvent(const QnResourcePtr& resource, qint64 timeStamp);
};

} // namespace event
} // namespace vms
} // namespace nx
