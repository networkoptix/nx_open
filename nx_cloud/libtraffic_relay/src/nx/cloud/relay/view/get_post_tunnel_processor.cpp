#include "get_post_tunnel_processor.h"

#include <nx/utils/log/log.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

namespace nx::cloud::relay::view {

GetPostTunnelProcessor::GetPostTunnelProcessor(
    const conf::Settings& settings,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool)
{
}

GetPostTunnelProcessor::~GetPostTunnelProcessor()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext: tunnelsInProgress)
        tunnelContext.second.connection->pleaseStopSync();
}

network::http::RequestResult GetPostTunnelProcessor::processOpenTunnelRequest(
    network::http::HttpServerConnection* const connection,
    const std::string& listeningPeerName,
    const network::http::Request& request,
    network::http::Response* const response)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open GET/POST tunnel request from peer %1 (%2)")
        .args(listeningPeerName, connection->socket()->getForeignAddress()));

    network::http::RequestResult requestResult(
        nx::network::http::StatusCode::ok);

    prepareCreateDownTunnelResponse(response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        std::bind(&GetPostTunnelProcessor::openUpTunnel, this,
            _1, listeningPeerName, request.requestLine.url.path().toStdString());

    return requestResult;
}

void GetPostTunnelProcessor::prepareCreateDownTunnelResponse(
    network::http::Response* const response)
{
    api::BeginListeningResponse responseData;
    responseData.preemptiveConnectionCount =
        m_settings.listeningPeer().recommendedPreemptiveConnectionCount;
    responseData.keepAliveOptions = m_settings.listeningPeer().tcpKeepAlive;

    serializeToHeaders(&response->headers, responseData);

    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Connection", "close");
}

void GetPostTunnelProcessor::openUpTunnel(
    network::http::HttpServerConnection* const connection,
    const std::string& listeningPeerName,
    const std::string& requestPath)
{
    using namespace std::placeholders;

    auto httpPipe = std::make_unique<network::http::AsyncMessagePipeline>(
        this,
        connection->takeSocket());
    auto httpPipePtr = httpPipe.get();

    QnMutexLocker lock(&m_mutex);

    auto insertionResult = m_tunnelsInProgress.emplace(
        httpPipePtr,
        TunnelContext{listeningPeerName, requestPath, std::move(httpPipe)});

    insertionResult.first->second.connection->setMessageHandler(
        std::bind(&GetPostTunnelProcessor::onMessage, this,
            insertionResult.first, _1));
    insertionResult.first->second.connection->startReadingConnection();
}

void GetPostTunnelProcessor::onMessage(
    Tunnels::iterator tunnelIter,
    network::http::Message message)
{
    if (!validateOpenUpChannelMessage(tunnelIter->second, message))
    {
        NX_DEBUG(this, lm("Invalid up channel message from peer %1 (%2)")
            .args(tunnelIter->second.listeningPeerName,
                tunnelIter->second.connection->socket()->getForeignAddress()));
        closeConnection(SystemError::invalidData, tunnelIter->first);
        return;
    }

    TunnelContext tunnelContext;
    {
        QnMutexLocker lock(&m_mutex);

        tunnelContext = std::move(tunnelIter->second);
        m_tunnelsInProgress.erase(tunnelIter);
    }

    NX_VERBOSE(this, lm("Successfully opened GET/POST tunnel for peer %1 (%2)")
        .args(tunnelContext.listeningPeerName,
            tunnelContext.connection->socket()->getForeignAddress()));

    m_listeningPeerPool->addConnection(
        tunnelContext.listeningPeerName,
        tunnelContext.connection->takeSocket());
}

bool GetPostTunnelProcessor::validateOpenUpChannelMessage(
    const TunnelContext& tunnelContext,
    const network::http::Message& message)
{
    if (message.type != network::http::MessageType::request ||
        message.request->requestLine.method != network::http::Method::post ||
        message.request->requestLine.url.path().toStdString() != tunnelContext.urlPath)
    {
        return false;
    }

    return true;
}

void GetPostTunnelProcessor::closeConnection(
    SystemError::ErrorCode /*closeReason*/,
    network::http::AsyncMessagePipeline* connection)
{
    QnMutexLocker lock(&m_mutex);

    m_tunnelsInProgress.erase(connection);
}

} // namespace nx::cloud::relay::view
