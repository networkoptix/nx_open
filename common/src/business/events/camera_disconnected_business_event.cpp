#include "camera_disconnected_business_event.h"
#include "core/resource/resource.h"

QnCameraDisconnectedBusinessEvent::QnCameraDisconnectedBusinessEvent(
        const QnResourcePtr& cameraResource,
        qint64 timeStamp):
    base_type(BusinessEventType::BE_Camera_Disconnect,
                            cameraResource,
                            timeStamp)
{
}

bool QnCameraDisconnectedBusinessEvent::checkCondition(ToggleState::Value state, const QnBusinessParams& params) const
{
    if (!base_type::checkCondition(state, params))
        return false;
    return true;
}

QString QnCameraDisconnectedBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    return text;
}
