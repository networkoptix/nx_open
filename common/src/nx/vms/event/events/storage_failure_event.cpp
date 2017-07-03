#include "storage_failure_event.h"
#include "core/resource/resource.h"

namespace nx {
namespace vms {
namespace event {

StorageFailureEvent::StorageFailureEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& storageUrl)
    :
    base_type(storageFailureEvent, resource, timeStamp, reasonCode, storageUrl)
{
}

} // namespace event
} // namespace vms
} // namespace nx
