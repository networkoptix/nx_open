#include "motion_business_event.h"
#include "core/resource/resource.h"

QnMotionBusinessEvent::QnMotionBusinessEvent(
        QnResourcePtr resource,
        ToggleState::Value toggleState,
        qint64 timeStamp,
        QnMetaDataV1Ptr metadata):
    QnAbstractBusinessEvent(BusinessEventType::BE_Camera_Motion,
                            resource,
                            toggleState,
                            timeStamp),
    m_metadata(metadata)
{
}
