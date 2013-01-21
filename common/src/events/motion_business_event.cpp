#include "motion_business_event.h"
#include "core/resource/resource.h"

QnMotionBusinessEvent::QnMotionBusinessEvent(
        const QnResourcePtr& resource,
        ToggleState::Value toggleState,
        qint64 timeStamp,
        QnAbstractDataPacketPtr metadata):
    base_type(BusinessEventType::BE_Camera_Motion,
                            resource,
                            toggleState,
                            timeStamp),
    m_metadata(metadata)
{
}
