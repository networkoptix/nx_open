#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode,
        QnResourcePtr storageResource):
    base_type(BusinessEventType::Storage_Failure,
                            resource,
                            timeStamp,
                            reasonCode)
{
    if (storageResource)
        m_reasonText = storageResource->getUrl();
}
