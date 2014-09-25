/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "camera_diagnostics_rest_handler.h"

#include <server/server_globals.h>

#include <api/model/camera_diagnostics_reply.h>
#include <core/dataprovider/spush_media_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/http/httptypes.h>

#include "camera/camera_pool.h"


static const QLatin1String resIDParamName("res_id");
static const QLatin1String diagnosticsTypeParamName("type");

int QnCameraDiagnosticsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams &params,
    QnJsonRestResult& result )
{
    QnUuid resID = QnUuid(params.value("res_id"));
    CameraDiagnostics::Step::Value diagnosticsType = CameraDiagnostics::Step::fromString(params.value("type"));
    if( resID.isNull() || diagnosticsType == CameraDiagnostics::Step::none )
        return nx_http::StatusCode::badRequest;
    
    //retrieving resource
    QnResourcePtr resource = QnResourcePool::instance()->getResourceById( resID );
    if( !resource )
        return nx_http::StatusCode::notFound;
    QnSecurityCamResourcePtr secCamRes = resource.dynamicCast<QnSecurityCamResource>();
    if( !secCamRes )
        return nx_http::StatusCode::badRequest;
    QnVideoCamera* videoCamera = QnVideoCameraPool::instance()->getVideoCamera( secCamRes );
    if( !videoCamera )
        return nx_http::StatusCode::notFound;

    QnCameraDiagnosticsReply reply;
    reply.performedStep = diagnosticsType;

    CameraDiagnostics::Result checkResult;
    //performing diagnostics on secCamRes
    switch( diagnosticsType )
    {
        case CameraDiagnostics::Step::cameraAvailability:
            checkResult = checkCameraAvailability( secCamRes );
            break;
        case CameraDiagnostics::Step::mediaStreamAvailability:
            checkResult = tryAcquireCameraMediaStream( secCamRes, videoCamera );
            break;
        case CameraDiagnostics::Step::mediaStreamIntegrity:
            checkResult = checkCameraMediaStreamForErrors( resource );
            break;
        default:
            return nx_http::StatusCode::notImplemented;
    }

    reply.errorCode = checkResult.errorCode;
    reply.errorParams = checkResult.errorParams;

    //serializing reply
    result.setReply( reply );

    return nx_http::StatusCode::ok;
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::checkCameraAvailability( const QnSecurityCamResourcePtr& cameraRes )
{
    if( !cameraRes->ping() )
        return CameraDiagnostics::CannotEstablishConnectionResult( cameraRes->httpPort() );

    if( cameraRes->initializationAttemptCount() == 0 )  //there was no attempt yet to initialize camera
        cameraRes->blockingInit();  //initializing camera to receive valid initialization result

    return cameraRes->prevInitializationResult();
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::tryAcquireCameraMediaStream(
    const QnSecurityCamResourcePtr& cameraRes,
    QnVideoCamera* videoCamera )
{
    Q_UNUSED(cameraRes)
    QnAbstractMediaStreamDataProviderPtr streamReader = videoCamera->getLiveReader(QnServer::HiQualityCatalog);
    if( !streamReader )
        return CameraDiagnostics::Result( CameraDiagnostics::ErrorCode::unknown, "no stream reader" ); //NOTE we should never get here 

    return streamReader->diagnoseMediaStreamConnection();
}

CameraDiagnostics::Result QnCameraDiagnosticsRestHandler::checkCameraMediaStreamForErrors( const QnResourcePtr& res )
{
    return res->getLastMediaIssue();
}
