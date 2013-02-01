#include "motion_business_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

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

bool QnMotionBusinessEvent::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled()
            && camera->getMotionType() != Qn::MT_NoMotion;

}

bool QnMotionBusinessEvent::isResourcesListValid(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return true; // should no check if any camera is selected
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            return false;
        }
    }
    return true;
}

int QnMotionBusinessEvent::invalidResourcesCount(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return 0; // should no check if any camera is selected
    int invalid = 0;
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            invalid++;
        }
    }
    return invalid;
}
