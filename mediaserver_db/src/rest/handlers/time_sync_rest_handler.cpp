#include "time_sync_rest_handler.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>

#include <rest/server/rest_connection_processor.h>

#include <local_connection_factory.h>
#include "managers/time_manager.h"

namespace ec2 {

QnTimeSyncRestHandler::QnTimeSyncRestHandler(LocalConnectionFactory* connection):
    m_appServerConnection(connection)
{
}

int QnTimeSyncRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* connection)
{
    const auto resultCode = processRequest(
        connection->request(),
        m_appServerConnection->timeSyncManager(),
        connection->socket().data());
    if (resultCode != nx::network::http::StatusCode::ok)
        return resultCode;

    // Sending our time synchronization information to remote peer.
    prepareResponse(
        *m_appServerConnection->timeSyncManager(),
        *m_appServerConnection->commonModule(),
        connection->response());

    return nx::network::http::StatusCode::ok;
}

int QnTimeSyncRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* connection)
{
    return executeGet(
        path,
        params,
        result,
        resultContentType,
        connection);
}

nx::network::http::StatusCode::Value QnTimeSyncRestHandler::processRequest(
    const nx::network::http::Request& request,
    TimeSynchronizationManager* timeSynchronizationManager,
    nx::network::AbstractStreamSocket* connection)
{
    auto peerGuid = request.headers.find(Qn::PEER_GUID_HEADER_NAME);
    if (peerGuid == request.headers.end())
        return nx::network::http::StatusCode::badRequest;
    auto timeSyncHeaderIter = request.headers.find(TimeSynchronizationManager::TIME_SYNC_HEADER_NAME);
    if (timeSyncHeaderIter != request.headers.end())
    {
        boost::optional<qint64> rttMillis;
        auto rttHeaderIter = request.headers.find(Qn::RTT_MS_HEADER_NAME);
        if (rttHeaderIter != request.headers.end())
            rttMillis = rttHeaderIter->second.toLongLong();

        if (!rttMillis)
        {
            nx::network::StreamSocketInfo sockInfo;
            if (connection && connection->getConnectionStatistics(&sockInfo))
                rttMillis = sockInfo.rttVar;
        }
        timeSynchronizationManager->processTimeSyncInfoHeader(
            QnUuid::fromStringSafe(peerGuid->second),
            timeSyncHeaderIter->second,
            rttMillis);
    }

    return nx::network::http::StatusCode::ok;
}

void QnTimeSyncRestHandler::prepareResponse(
    const TimeSynchronizationManager& timeSynchronizationManager,
    const QnCommonModule& commonModule,
    nx::network::http::Response* response)
{
    response->headers.emplace(
        TimeSynchronizationManager::TIME_SYNC_HEADER_NAME,
        timeSynchronizationManager.getTimeSyncInfo().toString());
    response->headers.emplace(
        Qn::PEER_GUID_HEADER_NAME,
        commonModule.moduleGUID().toByteArray());
}

} // namespace ec2
