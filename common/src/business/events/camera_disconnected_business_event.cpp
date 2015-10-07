#include "camera_disconnected_business_event.h"
#include "core/resource/resource.h"

QnCameraDisconnectedBusinessEvent::QnCameraDisconnectedBusinessEvent(
        const QnResourcePtr& cameraResource,
        qint64 timeStamp):
    base_type(QnBusiness::CameraDisconnectEvent,
                            cameraResource,
                            timeStamp)
{
}

bool QnCameraDisconnectedBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters& params, QnBusiness::ActionType actionType) const
{
    if (!base_type::checkCondition(state, params, actionType))
        return false;
    return true;
}
