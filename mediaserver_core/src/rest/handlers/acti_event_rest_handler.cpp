#ifdef ENABLE_ACTI


/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#include "acti_event_rest_handler.h"

#include <QStringList>

#include <core/resource_management/resource_pool.h>
#include <plugins/resource/acti/acti_resource.h>
#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>


int QnActiEventRestHandler::executeGet(
    const QString &path,
    const QnRequestParamList &params,
    QByteArray &responseMessageBody,
    QByteArray &contentType,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(responseMessageBody)
    Q_UNUSED(contentType)
    NX_ASSERT( path.indexOf("api/camera_event") != -1 );

    QStringList pathParts = path.split('/');
    if( pathParts.size() < 4 )
        return nx::network::http::StatusCode::badRequest; //missing resource id
    QnUuid resourceID(pathParts[3]);

    QnResourcePtr res = owner->resourcePool()->getResourceById(resourceID);
    if( !res )
        return nx::network::http::StatusCode::notFound;

    QnActiResourcePtr actiRes = res.dynamicCast<QnActiResource>();
    if( !actiRes )
        return nx::network::http::StatusCode::notImplemented;

    actiRes->cameraMessageReceived( path, params );

    return nx::network::http::StatusCode::ok;
}

int QnActiEventRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray& /*srcBodyContentType*/, QByteArray &, QByteArray &, const QnRestConnectionProcessor*)
{
    return nx::network::http::StatusCode::notImplemented;
}

#endif // #ifdef ENABLE_ACTI

