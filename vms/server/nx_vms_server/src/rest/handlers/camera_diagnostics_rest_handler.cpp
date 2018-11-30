#include "camera_diagnostics_rest_handler.h"

#include <server/server_globals.h>

#include <api/model/camera_diagnostics_reply.h>
#include <providers/spush_media_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_types.h>
#include <camera/camera_pool.h>
#include <api/helpers/camera_id_helper.h>
#include <nx/utils/log/log.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

#include <nx/vms/server/resource/camera.h>

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResIdParam = lit("res_id");
static const QStringList kCameraIdParams{kCameraIdParam, kDeprecatedResIdParam};
static const QString kTypeParam = lit("type");

} // namespace

QnCameraDiagnosticsRestHandler::QnCameraDiagnosticsRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

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
    NX_DEBUG(this, lit("received request %1").arg(path));

    CameraDiagnostics::Step::Value diagnosticsType =
        CameraDiagnostics::Step::fromString(params.value(kTypeParam));
    if (diagnosticsType == CameraDiagnostics::Step::none)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Invalid value of %1 parameter").arg(kTypeParam));
        return nx::network::http::StatusCode::badRequest;
    }

    QString notFoundCameraId = QString::null;
    const auto camera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->commonModule()->resourcePool(), &notFoundCameraId, params, kCameraIdParams)
        .dynamicCast<nx::vms::server::resource::Camera>();

    if (!camera)
    {
        if (!notFoundCameraId.isNull())
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Camera %1 not found").arg(notFoundCameraId));
            return nx::network::http::StatusCode::notFound;
        }
        else
        {
            result.setError(QnJsonRestResult::MissingParameter,
                lit("Parameter %1 is missing or invalid").arg(kCameraIdParam));
            return nx::network::http::StatusCode::badRequest;
        }
    }

    QnVideoCameraPtr videoCamera = videoCameraPool()->getVideoCamera(camera);
    if (!videoCamera)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Camera %1 is not a video camera").arg(camera->getId().toString()));
        return nx::network::http::StatusCode::notFound;
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
            checkResult = tryAcquireCameraMediaStream(videoCamera);
            break;
        case CameraDiagnostics::Step::mediaStreamIntegrity:
            checkResult = checkCameraMediaStreamForErrors(camera);
            break;
        default:
            result.setError(QnJsonRestResult::CantProcessRequest,
                lit("Diagnostics step %1 is not implemented").arg(
                    CameraDiagnostics::Step::toString(diagnosticsType)));
            return nx::network::http::StatusCode::notImplemented;
    }

    reply.errorCode = checkResult.errorCode;
    reply.errorParams = checkResult.errorParams;

    // Serializing reply.
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::checkCameraAvailability(
    const nx::vms::server::resource::CameraPtr& cameraRes)
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
    const nx::vms::server::resource::CameraPtr& camera)
{
    return camera->getLastMediaIssue();
}
