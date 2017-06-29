#include "license_issue_event.h"

namespace nx {
namespace vms {
namespace event {

LicenseIssueEvent::LicenseIssueEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(licenseIssueEvent, resource, timeStamp, reasonCode, reasonText)
{
}

} // namespace event
} // namespace vms
} // namespace nx
