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
