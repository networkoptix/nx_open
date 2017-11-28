#include "change_camera_password_rest_handler.h"

#include <api/model/password_data.h>
#include <core/resource/security_cam_resource.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

QnSecurityCamResourceList allCamerasInGroup(const QnSecurityCamResourcePtr& camera)
{
    QnResourcePtr server = camera->getParentServer();
    auto groupId = camera->getGroupId();
    if (groupId.isEmpty() || !server)
        return QnSecurityCamResourceList() << camera;
    return camera->resourcePool()->getAllCameras(server).filtered(
        [groupId](const QnVirtualCameraResourcePtr& camRes)
        {
            return camRes->getGroupId() == groupId;
        });
}

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
            lit("Missing required field '%1'").arg(missedField));
        return nx_http::StatusCode::ok;
    }

    auto requestedCamera = nx::camera_id_helper::findCameraByFlexibleId(
        owner->resourcePool(), data.cameraId);
    if (!requestedCamera)
    {
        result.setError(
            QnJsonRestResult::InvalidParameter,
            lit("Camera '%1' not found").arg(data.cameraId));
        return nx_http::StatusCode::ok;
    }

    if (requestedCamera->getParentId() != owner->commonModule()->moduleGUID())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This server is not a resource owner."));
        return nx_http::StatusCode::forbidden;
    }

    QAuthenticator auth;
    auth.setUser(data.user);
    auth.setPassword(data.password);
    QString errorString;
    if (!requestedCamera->setCameraCredentialsSync(auth, &errorString))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errorString);
        return nx_http::StatusCode::ok;
    }

    for (auto camera: allCamerasInGroup(requestedCamera))
    {
        camera->setAuth(auth);
        if (!camera->saveParams())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, "Internal server error");
            return nx_http::StatusCode::ok;
        }
        camera->reinitAsync();
    }
    return nx_http::StatusCode::ok;
}
