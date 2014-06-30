#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode,
        const QnResourcePtr& storageResource):
    base_type(QnBusiness::StorageFailureEvent,
                            resource,
                            timeStamp,
                            reasonCode)
{
    if (storageResource)
        m_reasonParamsEncoded = storageResource->getUrl();
}
