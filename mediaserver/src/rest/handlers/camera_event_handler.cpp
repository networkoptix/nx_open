#ifdef ENABLE_ACTI


/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_event_handler.h"

#include <QStringList>

#include <core/resource_management/resource_pool.h>
#include <plugins/resources/acti/acti_resource.h>
#include <utils/network/http/httptypes.h>


int QnCameraEventHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& responseMessageBody,
    QByteArray& contentType )
{
    Q_UNUSED(responseMessageBody)
    Q_UNUSED(contentType)
    Q_ASSERT( path.indexOf("api/camera_event") != -1 );

    const QStringList& pathParts = path.split('/');
    if( pathParts.size() < 3 )
        return nx_http::StatusCode::badRequest; //missing resource id 
    const QString& resourceID = pathParts[2];

    QnResourcePtr res = QnResourcePool::instance()->getResourceById(resourceID);
    if( !res )
        return nx_http::StatusCode::notFound;

    QnActiResourcePtr actiRes = res.dynamicCast<QnActiResource>();
    if( !actiRes )
        return nx_http::StatusCode::notImplemented;

    actiRes->cameraMessageReceived( path, params );

    return nx_http::StatusCode::ok;
}

int QnCameraEventHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& requestBody,
    QByteArray& responseMessageBody,
    QByteArray& contentType )
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(requestBody)
    Q_UNUSED(responseMessageBody)
    Q_UNUSED(contentType)
    return nx_http::StatusCode::notImplemented;
}

#endif // #ifdef ENABLE_ACTI

