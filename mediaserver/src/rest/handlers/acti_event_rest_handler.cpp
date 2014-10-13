#ifdef ENABLE_ACTI


/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#include "acti_event_rest_handler.h"

#include <QStringList>

#include <core/resource_management/resource_pool.h>
#include <plugins/resource/acti/acti_resource.h>
#include <utils/network/http/httptypes.h>


int QnActiEventRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &responseMessageBody, QByteArray &contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(responseMessageBody)
    Q_UNUSED(contentType)
    Q_ASSERT( path.indexOf("api/camera_event") != -1 );

    QStringList pathParts = path.split('/');
    if( pathParts.size() < 3 )
        return nx_http::StatusCode::badRequest; //missing resource id 
    QnUuid resourceID(pathParts[2]);

    QnResourcePtr res = QnResourcePool::instance()->getResourceById(resourceID);
    if( !res )
        return nx_http::StatusCode::notFound;

    QnActiResourcePtr actiRes = res.dynamicCast<QnActiResource>();
    if( !actiRes )
        return nx_http::StatusCode::notImplemented;

    actiRes->cameraMessageReceived( path, params );

    return nx_http::StatusCode::ok;
}

int QnActiEventRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray& /*srcBodyContentType*/, QByteArray &, QByteArray &, const QnRestConnectionProcessor*)
{
    return nx_http::StatusCode::notImplemented;
}

#endif // #ifdef ENABLE_ACTI

