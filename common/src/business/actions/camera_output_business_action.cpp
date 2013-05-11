#include "camera_output_business_action.h"

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnCameraOutputBusinessAction::QnCameraOutputBusinessAction(bool instant, const QnBusinessParams &runtimeParams):
    base_type(instant
              ? BusinessActionType::CameraOutputInstant
              : BusinessActionType::CameraOutput, runtimeParams)
{
}

QString QnCameraOutputBusinessAction::getRelayOutputId() const {
    return QnBusinessActionParameters::getRelayOutputId(getParams());
}

int QnCameraOutputBusinessAction::getRelayAutoResetTimeout() const {
    return QnBusinessActionParameters::getRelayAutoResetTimeout(getParams());
}

QString QnCameraOutputBusinessAction::getExternalUniqKey() const
{
    return QnAbstractBusinessAction::getExternalUniqKey() + QString(L'_') + getRelayOutputId();
}

bool QnCameraOutputBusinessAction::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

bool QnCameraOutputBusinessAction::isResourcesListValid(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return false;
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            return false;
        }
    }
    return true;
}

int QnCameraOutputBusinessAction::invalidResourcesCount(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = 0;
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            invalid++;
        }
    }
    return invalid;
}
