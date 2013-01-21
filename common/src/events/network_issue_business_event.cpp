#include "network_issue_business_event.h"
#include "core/resource/resource.h"

QnNetworkIssueBusinessEvent::QnNetworkIssueBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        const QString& reason):
    base_type(BusinessEventType::BE_Storage_Failure,
                            resource,
                            timeStamp),
    m_reason(reason)
{
}

QString QnNetworkIssueBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason%1\n").arg(m_reason);
    return text;
}
