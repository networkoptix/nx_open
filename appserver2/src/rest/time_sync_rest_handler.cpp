/**********************************************************
* Aug 14, 2015
* a.kolesnikov
***********************************************************/

#include "time_sync_rest_handler.h"

#include <http/custom_headers.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/network/http/httptypes.h>

#include "managers/time_manager.h"


namespace ec2 {

const QString QnTimeSyncRestHandler::PATH = QString::fromLatin1( "ec2/timeSync" );
const QByteArray QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME( "NX-TIME-SYNC-DATA" );

int QnTimeSyncRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* connection )
{
    auto timeSyncHeaderIter = connection->request().headers.find( TIME_SYNC_HEADER_NAME );
    if( timeSyncHeaderIter == connection->request().headers.end() )
        return nx_http::StatusCode::badRequest;
    auto peerGuid = connection->request().headers.find( Qn::PEER_GUID_HEADER_NAME );
    if( peerGuid == connection->request().headers.end() )
        return nx_http::StatusCode::badRequest;

    TimeSynchronizationManager::instance()->processTimeSyncInfoHeader(
        peerGuid->second,
        timeSyncHeaderIter->second,
        connection->socket().data() );

    //sending our time synchronization information to remote peer
    connection->response()->headers.emplace(
        TIME_SYNC_HEADER_NAME,
        TimeSynchronizationManager::instance()->getTimeSyncInfo().toString() );

    return nx_http::StatusCode::ok;
}

int QnTimeSyncRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* connection )
{
    return executeGet(
        path,
        params,
        result,
        resultContentType,
        connection );
}

}
