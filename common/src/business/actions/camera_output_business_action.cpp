#include "camera_output_business_action.h"

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnCameraOutputBusinessAction::QnCameraOutputBusinessAction(bool instant, const QnBusinessEventParameters &runtimeParams):
    base_type(instant
              ? BusinessActionType::CameraOutputInstant
              : BusinessActionType::CameraOutput, runtimeParams)
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

bool QnCameraOutputAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

QString QnCameraOutputAllowedPolicy::getErrorText(int invalid, int total) {
    return tr("%1 of %2 selected cameras have not output relays.").arg(invalid).arg(total);
}
