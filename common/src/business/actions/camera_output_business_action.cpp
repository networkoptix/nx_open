#include "camera_output_business_action.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace BusinessActionParameters {
    static QLatin1String relayOutputId("relayOutputID");
    static QLatin1String relayAutoResetTimeout("relayAutoResetTimeout");

    QString getRelayOutputId(const QnBusinessParams &params) {
        return params.value(relayOutputId, QString()).toString();
    }

    void setRelayOutputId(QnBusinessParams* params, const QString &value) {
        (*params)[relayOutputId] = value;
    }

    int getRelayAutoResetTimeout(const QnBusinessParams &params) {
        return params.value(relayAutoResetTimeout, 0).toInt();
    }

    void setRelayAutoResetTimeout(QnBusinessParams* params, int value) {
        (*params)[relayAutoResetTimeout] = value;
    }

}


QnCameraOutputBusinessAction::QnCameraOutputBusinessAction(bool instant, const QnBusinessParams &runtimeParams):
    base_type(instant
              ? BusinessActionType::CameraOutputInstant
              : BusinessActionType::CameraOutput, runtimeParams)
{
}

QString QnCameraOutputBusinessAction::getRelayOutputId() const {
    return BusinessActionParameters::getRelayOutputId(getParams());
}

int QnCameraOutputBusinessAction::getRelayAutoResetTimeout() const {
    return BusinessActionParameters::getRelayAutoResetTimeout(getParams());
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
