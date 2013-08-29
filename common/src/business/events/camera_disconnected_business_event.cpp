#include "camera_disconnected_business_event.h"
#include "core/resource/resource.h"

QnCameraDisconnectedBusinessEvent::QnCameraDisconnectedBusinessEvent(
        const QnResourcePtr& cameraResource,
        qint64 timeStamp):
    base_type(BusinessEventType::Camera_Disconnect,
                            cameraResource,
                            timeStamp)
{
}

bool QnCameraDisconnectedBusinessEvent::checkCondition(Qn::ToggleState state, const QnBusinessEventParameters& params) const
{
    if (!base_type::checkCondition(state, params))
        return false;
    return true;
}
