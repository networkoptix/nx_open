/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "camera_diagnostics_handler.h"

#include <api/model/camera_diagnostics_reply.h>
#include <core/dataprovider/spush_media_stream_provider.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/http/httptypes.h>

#include "camera/camera_pool.h"


static const QLatin1String resIDParamName("res_id");
static const QLatin1String diagnosticsTypeParamName("type");

QnCameraDiagnosticsHandler::QnCameraDiagnosticsHandler()
{
}

int QnCameraDiagnosticsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList &params,
    JsonResult& result )
{
    QString resID;
    CameraDiagnostics::Step::Value diagnosticsType = CameraDiagnostics::Step::none;
    for( QnRequestParamList::const_iterator
        it = params.begin();
        it != params.end();
        ++it )
    {
        if( it->first == resIDParamName )
            resID = it->second;
        else if( it->first == diagnosticsTypeParamName )
            diagnosticsType = CameraDiagnostics::Step::fromString(it->second);
    }

    if( resID.isEmpty() || diagnosticsType == CameraDiagnostics::Step::none )
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
            checkResult = checkCameraMediaStreamForErrors( videoCamera );
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

QString QnCameraDiagnosticsHandler::description() const
{
    QString diagnosticsTypeStrList;
    for( int i = CameraDiagnostics::Step::none+1; i < CameraDiagnostics::Step::end; ++i )
    {
        if( !diagnosticsTypeStrList.isEmpty() )
            diagnosticsTypeStrList += QLatin1String(", ");
        diagnosticsTypeStrList += CameraDiagnostics::Step::toString(static_cast<CameraDiagnostics::Step::Value>(i));
    }

    return QString::fromLatin1(
        "Performs camera diagnostics"
        "<BR>Param <b>%1</b> - Required. ID of camera"
        "<BR>Param <b>%2</b> - Diagnostics to perform (%3)").
        arg(resIDParamName).arg(diagnosticsTypeParamName).arg(diagnosticsTypeStrList);
}

CameraDiagnostics::Result QnCameraDiagnosticsHandler::checkCameraAvailability( const QnSecurityCamResourcePtr& cameraRes )
{
    if( !cameraRes->ping() )
        return CameraDiagnostics::CannotEstablishConnectionResult( cameraRes->httpPort() );

    return cameraRes->prevInitializationResult();
}

CameraDiagnostics::Result QnCameraDiagnosticsHandler::tryAcquireCameraMediaStream(
    const QnSecurityCamResourcePtr& cameraRes,
    QnVideoCamera* videoCamera )
{
    QnAbstractMediaStreamDataProviderPtr streamReader = videoCamera->getLiveReader( QnResource::Role_LiveVideo );
    if( !streamReader )
        return CameraDiagnostics::Result(
            CameraDiagnostics::ErrorCode::cannotConfigureMediaStream,
            cameraRes->getHostAddress() );

    return streamReader->diagnoseMediaStreamConnection();
}

CameraDiagnostics::Result QnCameraDiagnosticsHandler::checkCameraMediaStreamForErrors( QnVideoCamera* /*videoCamera*/ )
{
    //TODO/IMPL
    return CameraDiagnostics::NoErrorResult();
}
