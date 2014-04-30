#include "network_issue_business_event.h"
#include "core/resource/resource.h"

QnNetworkIssueBusinessEvent::QnNetworkIssueBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode,
        const QString &reasonParamsEncoded):
    base_type(QnBusiness::NetworkIssueEvent,
              resource,
              timeStamp,
              reasonCode,
              reasonParamsEncoded)

{
}

int QnNetworkIssueBusinessEvent::decodeTimeoutMsecs(const QString &encoded, const int defaultValue) {
    bool ok;
    int result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString QnNetworkIssueBusinessEvent::encodeTimeoutMsecs(int msecs) {
    return QString::number(msecs);
}

bool QnNetworkIssueBusinessEvent::decodePrimaryStream(const QString &encoded, const bool defaultValue) {
    bool ok;
    bool result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString QnNetworkIssueBusinessEvent::encodePrimaryStream(bool isPrimary) {
    return QString::number(isPrimary);
}

QnNetworkIssueBusinessEvent::PacketLossSequence QnNetworkIssueBusinessEvent::decodePacketLossSequence(const QString &encoded) {
    QnNetworkIssueBusinessEvent::PacketLossSequence result;
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

QString QnNetworkIssueBusinessEvent::encodePacketLossSequence(int prev, int next) {
    return QString(QLatin1String("%1;%2")).arg(prev).arg(next);
}
