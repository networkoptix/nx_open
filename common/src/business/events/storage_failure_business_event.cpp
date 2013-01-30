#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnResourcePtr storageResource,
        const QString& reason):
    base_type(BusinessEventType::BE_Storage_Failure,
                            resource,
                            timeStamp),
    m_reason(reason),
    m_storageResource(storageResource)
{
}

QString QnStorageFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason%1\n").arg(m_reason);
    if (m_storageResource)
        text += QObject::tr("  storage%1\n").arg(m_storageResource->getUrl());
    else
        text += QObject::tr("  no storage for writting\n");
    return text;
}
