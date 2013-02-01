#include "mserver_failure_business_event.h"
#include "core/resource/resource.h"

QnMServerFailureBusinessEvent::QnMServerFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp):
    base_type(BusinessEventType::BE_MediaServer_Failure,
                            resource,
                            timeStamp)
{
}

QString QnMServerFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    return text;
}
