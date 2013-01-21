#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        const QString& reason):
    base_type(BusinessEventType::BE_Storage_Failure,
                            resource,
                            timeStamp),
    m_reason(reason)
{
}

QString QnStorageFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason%1\n").arg(m_reason);
    return text;
}
