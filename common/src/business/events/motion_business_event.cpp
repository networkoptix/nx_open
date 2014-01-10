#include "motion_business_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnMotionBusinessEvent::QnMotionBusinessEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata):
    base_type(BusinessEventType::Camera_Motion, resource, toggleState, timeStamp),
    m_metadata(metadata)
{
}
