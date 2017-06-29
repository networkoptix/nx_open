#include "network_issue_event.h"

#include <core/resource/resource.h>

namespace nx {
namespace vms {
namespace event {

NetworkIssueEvent::NetworkIssueEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonParamsEncoded)
    :
    base_type(networkIssueEvent, resource, timeStamp, reasonCode, reasonParamsEncoded)
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

NetworkIssueEvent::PacketLossSequence NetworkIssueEvent::decodePacketLossSequence(const QString& encoded)
{
    NetworkIssueEvent::PacketLossSequence result;
    result.valid = false;

    QStringList seqs = encoded.split(L';');
    if (seqs.size() != 2)
        return result;

    bool ok1, ok2;
    result.prev = seqs[0].toInt(&ok1);
    result.next = seqs[1].toInt(&ok2);
    result.valid = ok1 && ok2;
    return result;
}

QString NetworkIssueEvent::encodePacketLossSequence(int prev, int next)
{
    return QString(QLatin1String("%1;%2")).arg(prev).arg(next);
}

} // namespace event
} // namespace vms
} // namespace nx
