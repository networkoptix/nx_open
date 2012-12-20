#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        QnResourcePtr resource,
        qint64 timeStamp):
    QnAbstractBusinessEvent(BusinessEventType::BE_Storage_Failure,
                            resource,
                            ToggleState::NotDefined,
                            timeStamp)
{
}

QString QnStorageFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    //text += tr("  storage %1\n").arg(m_cameraResource->getUniqueId());
    return text;
}
