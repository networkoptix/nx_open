#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

namespace QnBusinessEventRuntime
{
    static QLatin1String storageUrlStr("storageUrl");

    QString getStorageResourceUrl(const QnBusinessParams &params)
    {
        return params.value(storageUrlStr, QString()).toString();
    }

    void setStorageResourceUrl(QnBusinessParams* params, QString value)
    {
        (*params)[storageUrlStr] = value;
    }
}

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
