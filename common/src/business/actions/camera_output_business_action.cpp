#include "camera_output_business_action.h"

#include <business/business_action_parameters.h>

QnCameraOutputBusinessAction::QnCameraOutputBusinessAction(const QnBusinessEventParameters &runtimeParams) :
    base_type(QnBusiness::CameraOutputAction, runtimeParams)
{
//     if (instant)
//         m_params.relayAutoResetTimeout = QnCameraOutputBusinessAction::kInstantActionAutoResetTimeoutMs; // default value for instant action
}

QString QnCameraOutputBusinessAction::getRelayOutputId() const
{
    return m_params.relayOutputId;
}

int QnCameraOutputBusinessAction::getRelayAutoResetTimeout() const
{
    return m_params.durationMs;
}

QString QnCameraOutputBusinessAction::getExternalUniqKey() const
{
    return QnAbstractBusinessAction::getExternalUniqKey() + QString(L'_') + getRelayOutputId();
}
