#include "motion_business_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnMotionBusinessEvent::QnMotionBusinessEvent(const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata):
    base_type(QnBusiness::CameraMotionEvent, resource, toggleState, timeStamp),
    m_metadata(metadata)
{
}
