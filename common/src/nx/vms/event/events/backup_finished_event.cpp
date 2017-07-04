#include "backup_finished_event.h"

namespace nx {
namespace vms {
namespace event {

BackupFinishedEvent::BackupFinishedEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(backupFinishedEvent, resource, timeStamp, reasonCode, reasonText)
{
}

} // namespace event
} // namespace vms
} // namespace nx
