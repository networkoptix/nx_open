#include "network_issue_event.h"

#include <nx/fusion/model_functions.h>

#include <core/resource/resource.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters, (json),
    MulticastAddressConflictParameters_Fields, (brief, true))

NetworkIssueEvent::NetworkIssueEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonParamsEncoded)
    :
    base_type(EventType::networkIssueEvent, resource, timeStamp, reasonCode, reasonParamsEncoded)
{
}

int NetworkIssueEvent::decodeTimeoutMsecs(const QString& encoded, const int defaultValue)
{
    bool ok;
    int result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString NetworkIssueEvent::encodeTimeoutMsecs(int msecs)
{
    return QString::number(msecs);
}

bool NetworkIssueEvent::decodePrimaryStream(const QString& encoded, const bool defaultValue)
{
    bool ok;
    bool result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString NetworkIssueEvent::encodePrimaryStream(bool isPrimary) {
    return QString::number(isPrimary);
}

} // namespace event
} // namespace vms
} // namespace nx
