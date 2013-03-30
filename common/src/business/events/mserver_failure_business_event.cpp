#include "mserver_failure_business_event.h"
#include "core/resource/resource.h"

QnMServerFailureBusinessEvent::QnMServerFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode):
    base_type(BusinessEventType::MediaServer_Failure,
                            resource,
                            timeStamp,
                            reasonCode)
{
}
