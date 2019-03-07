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

QString NetworkIssueEvent::PacketLoss::toString() const
{
    const auto loss = QString("%1;%2").arg(prev).arg(next);
    if (!aggregated)
        return loss;

    return QString("%1;%2").arg(loss).arg(aggregated);
}

std::optional<NetworkIssueEvent::PacketLoss>
    NetworkIssueEvent::PacketLoss::parse(const QString& string)
{
    PacketLoss result;
    QStringList parts = string.split(L';');
    if (parts.size() < 2)
        return std::nullopt;

    bool isOk = false;
    result.prev = parts[0].toULongLong(&isOk);
    if (!isOk)
        return std::nullopt;

    result.next = parts[1].toULongLong(&isOk);
    if (!isOk)
        return std::nullopt;

    if (parts.size() > 2)
    {
        result.aggregated = parts[2].toULongLong(&isOk);
        if (!isOk)
            return std::nullopt;
    }

    return result;
}

} // namespace event
} // namespace vms
} // namespace nx
