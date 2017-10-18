#include "change_camera_password_rest_handler.h"

#include <api/model/password_data.h>
#include <core/resource/security_cam_resource.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/helpers/camera_id_helper.h>

int QnChangeCameraPasswordRestHandler::executePost(
        const QString& /*path*/,
        const QnRequestParams& /*params*/,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner)
{
    bool success = false;
    auto data = QJson::deserialized<CameraPasswordData>(body, CameraPasswordData(), &success);
    if (!success)
    {
        result.setError(
            QnJsonRestResult::InvalidParameter,
            lit("Invalid Json object provided"));
        return nx_http::StatusCode::ok;
    }

    QString missedField;
    if (data.cameraId.isEmpty())
        missedField = "cameraId";
    else if (data.user.isEmpty())
        missedField = "user";
    else if (data.password.isEmpty())
        missedField = "password";
    if (!missedField.isEmpty())
    {
        result.setError(
            QnJsonRestResult::InvalidParameter,
            lit("Missing required field %1").arg(missedField));
        return nx_http::StatusCode::ok;
    }

    auto camera = nx::camera_id_helper::findCameraByFlexibleId(
        owner->resourcePool(), data.cameraId);
    if (!camera)
    {
        result.setError(
            QnJsonRestResult::InvalidParameter,
            lit("Camera '%1' not found").arg(data.cameraId));
        return nx_http::StatusCode::ok;
    }

    QAuthenticator auth;
    QString errorString;
    if (!camera->setCameraCredentialsSync(auth, &errorString))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            errorString);
        return nx_http::StatusCode::ok;
    }

    return nx_http::StatusCode::ok;
}
