#include "motion_business_event.h"
#include "core/resource/resource.h"

QnMotionBusinessEvent::QnMotionBusinessEvent(
        QnResourcePtr resource,
        ToggleState::Value toggleState,
        qint64 timeStamp):
    QnAbstractBusinessEvent(BusinessEventType::BE_Camera_Motion,
                            resource,
                            toggleState,
                            timeStamp)
{
}
