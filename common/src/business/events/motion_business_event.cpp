#include "motion_business_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnMotionBusinessEvent::QnMotionBusinessEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata):
    base_type(BusinessEventType::Camera_Motion, resource, toggleState, timeStamp),
    m_metadata(metadata)
{
}

bool QnCameraMotionAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled()
            && camera->getMotionType() != Qn::MT_NoMotion
            && camera->supportedMotionType() != Qn::MT_NoMotion;
}

QString QnCameraMotionAllowedPolicy::getErrorText(int invalid, int total) {
    return tr("Recording or motion detection is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
}
