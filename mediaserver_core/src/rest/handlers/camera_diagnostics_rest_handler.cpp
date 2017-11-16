#include "camera_diagnostics_rest_handler.h"

#include <server/server_globals.h>

#include <api/model/camera_diagnostics_reply.h>
#include <providers/spush_media_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/http/http_types.h>
#include <camera/camera_pool.h>
#include <api/helpers/camera_id_helper.h>
#include <nx/utils/log/log.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResIdParam = lit("res_id");
static const QStringList kCameraIdParams{kCameraIdParam, kDeprecatedResIdParam};
static const QString kTypeParam = lit("type");

} // namespace

QStringList QnCameraDiagnosticsRestHandler::cameraIdUrlParams() const
{
    return kCameraIdParams;
}

int QnCameraDiagnosticsRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_LOG(lit("QnCameraDiagnosticsRestHandler: received request %1").arg(path), cl_logDEBUG1);

    CameraDiagnostics::Step::Value diagnosticsType =
        CameraDiagnostics::Step::fromString(params.value(kTypeParam));
    if (diagnosticsType == CameraDiagnostics::Step::none)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Invalid value of %1 parameter").arg(kTypeParam));
        return nx_http::StatusCode::badRequest;
    }

    QString notFoundCameraId = QString::null;
    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->commonModule()->resourcePool(), &notFoundCameraId, params, kCameraIdParams);
    if (!camera)
    {
        if (!notFoundCameraId.isNull())
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Camera %1 not found").arg(notFoundCameraId));
            return nx_http::StatusCode::notFound;
        }
        else
        {
            result.setError(QnJsonRestResult::MissingParameter,
                lit("Parameter %1 is missing or invalid").arg(kCameraIdParam));
            return nx_http::StatusCode::badRequest;
        }
    }

    QnVideoCameraPtr videoCamera = QnVideoCameraPool::instance()->getVideoCamera(camera);
    if (!videoCamera)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Camera %1 is not a video camera").arg(camera->getId().toString()));
        return nx_http::StatusCode::notFound;
    }

    QnCameraDiagnosticsReply reply;
    reply.performedStep = diagnosticsType;

    // Performing diagnostics on secCamRes.
    CameraDiagnostics::Result checkResult;
    switch (diagnosticsType)
    {
        case CameraDiagnostics::Step::cameraAvailability:
            checkResult = checkCameraAvailability(camera);
            break;
        case CameraDiagnostics::Step::mediaStreamAvailability:
            checkResult = tryAcquireCameraMediaStream(camera, videoCamera);
            break;
        case CameraDiagnostics::Step::mediaStreamIntegrity:
            checkResult = checkCameraMediaStreamForErrors(camera);
            break;
        default:
            result.setError(QnJsonRestResult::CantProcessRequest,
                lit("Diagnostics step %1 is not implemented").arg(
                    CameraDiagnostics::Step::toString(diagnosticsType)));
            return nx_http::StatusCode::notImplemented;
    }

    reply.errorCode = checkResult.errorCode;
    reply.errorParams = checkResult.errorParams;

    // Serializing reply.
    result.setReply(reply);

    return nx_http::StatusCode::ok;
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::checkCameraAvailability(
    const QnSecurityCamResourcePtr& cameraRes)
{
    if (!cameraRes->ping())
        return CameraDiagnostics::CannotEstablishConnectionResult(cameraRes->httpPort());

    if (cameraRes->initializationAttemptCount() == 0)
    {
        // There was no attempt yet to initialize the camera.
        cameraRes->blockingInit(); // Initialize the camera to receive valid initialization result.
    }

    return cameraRes->prevInitializationResult();
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::tryAcquireCameraMediaStream(
    const QnSecurityCamResourcePtr& /*cameraRes*/,
    const QnVideoCameraPtr& videoCamera)
{
    QnAbstractMediaStreamDataProviderPtr streamReader =
        videoCamera->getLiveReader(QnServer::HiQualityCatalog);
    if (!streamReader)
    {
        return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::unknown,
            "no stream reader"); //< NOTE: We should never get here.
    }

    return streamReader->diagnoseMediaStreamConnection();
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::checkCameraMediaStreamForErrors(
    const QnResourcePtr& res)
{
    return res->getLastMediaIssue();
}
