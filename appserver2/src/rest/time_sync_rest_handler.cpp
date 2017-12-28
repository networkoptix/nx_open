/**********************************************************
* Aug 14, 2015
* a.kolesnikov
***********************************************************/

#include "time_sync_rest_handler.h"

#include <nx/network/http/custom_headers.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/network/http/http_types.h>

#include "managers/time_manager.h"
#include "connection_factory.h"


namespace ec2 {

const QString QnTimeSyncRestHandler::PATH = QString::fromLatin1( "ec2/timeSync" );
const QByteArray QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME( "NX-TIME-SYNC-DATA" );

QnTimeSyncRestHandler::QnTimeSyncRestHandler(Ec2DirectConnectionFactory* connection) :
    m_appServerConnection(connection)
{
}

int QnTimeSyncRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* connection )
{
    auto peerGuid = connection->request().headers.find( Qn::PEER_GUID_HEADER_NAME );
    if( peerGuid == connection->request().headers.end() )
        return nx::network::http::StatusCode::badRequest;
    auto timeSyncHeaderIter = connection->request().headers.find( TIME_SYNC_HEADER_NAME );
    if (timeSyncHeaderIter != connection->request().headers.end())
    {
        boost::optional<qint64> rttMillis;
        auto rttHeaderIter = connection->request().headers.find(Qn::RTT_MS_HEADER_NAME);
        if (rttHeaderIter != connection->request().headers.end())
            rttMillis = rttHeaderIter->second.toLongLong();

        if (!rttMillis)
        {
            nx::network::StreamSocketInfo sockInfo;
            if (connection->socket() && connection->socket()->getConnectionStatistics(&sockInfo))
                rttMillis = sockInfo.rttVar;
        }
        m_appServerConnection->timeSyncManager()->processTimeSyncInfoHeader(
            QnUuid::fromStringSafe(peerGuid->second),
            timeSyncHeaderIter->second,
            rttMillis);
    }

    //sending our time synchronization information to remote peer
    connection->response()->headers.emplace(
        TIME_SYNC_HEADER_NAME,
        m_appServerConnection->timeSyncManager()->getTimeSyncInfo().toString() );
    connection->response()->headers.emplace(
        Qn::PEER_GUID_HEADER_NAME,
        m_appServerConnection->commonModule()->moduleGUID().toByteArray() );

    return nx::network::http::StatusCode::ok;
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
