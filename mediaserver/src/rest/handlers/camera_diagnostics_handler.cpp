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
    const QnRequestParamList& params,
    QByteArray& responseMessageBody,
    QByteArray& contentType )
{
    QString resID;
    CameraDiagnostics::DiagnosticsStep::Value diagnosticsType = CameraDiagnostics::DiagnosticsStep::none;
    for( QnRequestParamList::const_iterator
        it = params.begin();
        it != params.end();
        ++it )
    {
        if( it->first == resIDParamName )
            resID = it->second;
        else if( it->first == diagnosticsTypeParamName )
            diagnosticsType = CameraDiagnostics::DiagnosticsStep::fromString(it->second);
    }

    if( resID.isEmpty() || diagnosticsType == CameraDiagnostics::DiagnosticsStep::none )
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

    //performing diagnostics on secCamRes
    switch( diagnosticsType )
    {
        case CameraDiagnostics::DiagnosticsStep::cameraAvailability:
            reply.errorCode = checkCameraAvailability( secCamRes, &reply.errorParams );
            break;
        case CameraDiagnostics::DiagnosticsStep::mediaStreamAvailability:
            reply.errorCode = tryAcquireCameraMediaStream( secCamRes, videoCamera, &reply.errorParams );
            break;
        case CameraDiagnostics::DiagnosticsStep::mediaStreamIntegrity:
            reply.errorCode = checkCameraMediaStreamForErrors( videoCamera, &reply.errorParams );
            break;
        default:
            return nx_http::StatusCode::notImplemented;
    }

    //serializing reply
    QVariant responseData;
    serialize( reply, &responseData );
    responseMessageBody = responseData.toByteArray();

    contentType = "text/plain";

    return nx_http::StatusCode::ok;
}

int QnCameraDiagnosticsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/ )
{
    //TODO/IMPL
    return nx_http::StatusCode::notImplemented;
}

QString QnCameraDiagnosticsHandler::description() const
{
    QString diagnosticsTypeStrList;
    for( int i = CameraDiagnostics::DiagnosticsStep::none+1; i < CameraDiagnostics::DiagnosticsStep::end; ++i )
    {
        if( !diagnosticsTypeStrList.isEmpty() )
            diagnosticsTypeStrList += QLatin1String(", ");
        diagnosticsTypeStrList += CameraDiagnostics::DiagnosticsStep::toString(static_cast<CameraDiagnostics::DiagnosticsStep::Value>(i));
    }

    return QString::fromLatin1(
        "Performs camera diagnostics"
        "<BR>Param <b>%1</b> - Required. ID of camera"
        "<BR>Param <b>%2</b> - Diagnostics to perform (%3)").
        arg(resIDParamName).arg(diagnosticsTypeParamName).arg(diagnosticsTypeStrList);
}

CameraDiagnostics::ErrorCode::Value QnCameraDiagnosticsHandler::checkCameraAvailability(
    const QnSecurityCamResourcePtr& cameraRes,
    QList<QString>* const errorParams )
{
    if( !cameraRes->ping() )
    {
        errorParams->push_back( cameraRes->getHostAddress() );
        return CameraDiagnostics::ErrorCode::cannotEstablishConnection;
    }

    return CameraDiagnostics::ErrorCode::noError;
}

CameraDiagnostics::ErrorCode::Value QnCameraDiagnosticsHandler::tryAcquireCameraMediaStream(
    const QnSecurityCamResourcePtr& cameraRes,
    QnVideoCamera* videoCamera,
    QList<QString>* const errorParams )
{
    QnAbstractMediaStreamDataProviderPtr liveDataProvider = videoCamera->getLiveReader( QnResource::Role_LiveVideo );
    if( !liveDataProvider )
        return CameraDiagnostics::ErrorCode::cannotOpenCameraMediaPort;

    //return cameraRes->;
    return CameraDiagnostics::ErrorCode::noError;
}

CameraDiagnostics::ErrorCode::Value QnCameraDiagnosticsHandler::checkCameraMediaStreamForErrors(
    QnVideoCamera* videoCamera,
    QList<QString>* const errorParams )
{
    //TODO/IMPL
    return CameraDiagnostics::ErrorCode::noError;
}
