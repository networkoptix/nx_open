#include <api/helpers/camera_id_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/vms/server/resource/camera.h>

#include "camera_manifest_rest_handler.h"

static const QString kCameraIdParam = "cameraId";
using StatusCode = nx::network::http::StatusCode::Value;

int QnCameraManifestRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const auto cameraId = params.value(kCameraIdParam);
    if (cameraId.isEmpty())
    {
        result.setError(
            QnRestResult::MissingParameter, NX_FMT("%1 is not specified or empty", kCameraIdParam));
        return StatusCode::unprocessableEntity;
    }
    const auto camera =
        nx::camera_id_helper::findCameraByFlexibleId(owner->resourcePool(), cameraId);
    if (!camera)
    {
        result.setError(QnRestResult::InvalidParameter, NX_FMT("Camera %1 not found", cameraId));
        return StatusCode::unprocessableEntity;
    }
    const auto serverCamera = camera.dynamicCast<nx::vms::server::resource::Camera>();
    if (!serverCamera)
    {
        result.setError(QnRestResult::InternalServerError, "Camera resource is invalid");
        return StatusCode::internalServerError;
    }
    result.setReply(serverCamera->getAdvancedParametersManifest());
    return StatusCode::ok;
}
