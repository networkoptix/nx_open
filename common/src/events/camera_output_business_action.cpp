#include "camera_output_business_action.h"

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


QnCameraOutputBusinessAction::QnCameraOutputBusinessAction():
    base_type(BusinessActionType::BA_CameraOutput)
{
}

QString QnCameraOutputBusinessAction::getRelayOutputId() const {
    return BusinessActionParameters::getRelayOutputId(getParams());
}

int QnCameraOutputBusinessAction::getRelayAutoResetTimeout() const {
    return BusinessActionParameters::getRelayAutoResetTimeout(getParams());
}
