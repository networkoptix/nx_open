#include "network_issue_business_event.h"
#include "core/resource/resource.h"

QnNetworkIssueBusinessEvent::QnNetworkIssueBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        int reasonCode,
        const QString &reasonText):
    base_type(BusinessEventType::BE_Network_Issue,
              resource,
              timeStamp,
              reasonCode,
              reasonText)

{
}

QString QnNetworkIssueBusinessEvent::toString() const
{
    QString reasonText;
    switch (m_reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            reasonText = QObject::tr("No video frame during %1 seconds").arg(m_reasonText);
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            reasonText = QObject::tr("Connection to camera was unexpectedly closed");
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            {
                QStringList seqs = m_reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                reasonText = QObject::tr("RTP packet loss detected. Prev seq.=%1 next seq.=%2")
                        .arg(seqs[0])
                        .arg(seqs[1]);
            }
            break;
        default:
            break;

    }

    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason: %1\n").arg(reasonText);
    return text;
}
