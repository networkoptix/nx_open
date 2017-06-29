#include "server_started_event.h"

namespace nx {
namespace vms {
namespace event {

ServerStartedEvent::ServerStartedEvent(const QnResourcePtr& resource, qint64 timeStamp):
    base_type(serverStartEvent, resource, timeStamp)
{
}

} // namespace event
} // namespace vms
} // namespace nx
