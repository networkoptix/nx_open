#include "server_failure_event.h"

namespace nx {
namespace vms {
namespace event {

ServerFailureEvent::ServerFailureEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(serverFailureEvent, resource, timeStamp, reasonCode, reasonText)
{
}

} // namespace event
} // namespace vms
} // namespace nx
