#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode,
        const QString& storageUrl):
    base_type(QnBusiness::StorageFailureEvent,
                            resource,
                            timeStamp,
                            reasonCode)
{
    m_reasonParamsEncoded = storageUrl;
}
