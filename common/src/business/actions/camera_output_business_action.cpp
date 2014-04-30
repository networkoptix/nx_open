#include "camera_output_business_action.h"

#include <business/business_action_parameters.h>

QnCameraOutputBusinessAction::QnCameraOutputBusinessAction(bool instant, const QnBusinessEventParameters &runtimeParams):
    base_type(instant
              ? QnBusiness::CameraOutputOnceAction
              : QnBusiness::CameraOutputAction, runtimeParams)
{
}

QString QnCameraOutputBusinessAction::getRelayOutputId() const {
    return m_params.getRelayOutputId();
}

int QnCameraOutputBusinessAction::getRelayAutoResetTimeout() const {
    return m_params.getRelayAutoResetTimeout();
}

QString QnCameraOutputBusinessAction::getExternalUniqKey() const
{
    return QnAbstractBusinessAction::getExternalUniqKey() + QString(L'_') + getRelayOutputId();
}
