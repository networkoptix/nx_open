#include "get_post_tunnel_processor.h"

#include <nx/utils/log/log.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

namespace nx::cloud::relay::view {

template<typename RequestSpecificData>
GetPostTunnelProcessor<RequestSpecificData>::GetPostTunnelProcessor(
    const conf::Settings& settings)
    :
    m_settings(settings)
{
}

template<typename RequestSpecificData>
GetPostTunnelProcessor<RequestSpecificData>::~GetPostTunnelProcessor()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext: tunnelsInProgress)
        tunnelContext.second.connection->pleaseStopSync();
}

template<typename RequestSpecificData>
network::http::RequestResult
    GetPostTunnelProcessor<RequestSpecificData>::processOpenTunnelRequest(
        RequestSpecificData requestData,
        const network::http::Request& request,
        network::http::Response* const response)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open GET/POST tunnel. Url %1")
        .args(request.requestLine.url.path()));

    network::http::RequestResult requestResult(
        nx::network::http::StatusCode::ok);

    prepareCreateDownTunnelResponse(response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::move(requestData),
            requestPath = request.requestLine.url.path().toStdString()](
                network::http::HttpServerConnection* connection) mutable
        {
            openUpTunnel(
                connection,
                std::move(requestData),
                requestPath);
        };

    return requestResult;
}

template<typename RequestSpecificData>
void GetPostTunnelProcessor<RequestSpecificData>::prepareCreateDownTunnelResponse(
    network::http::Response* const response)
{
    //--
    api::BeginListeningResponse responseData;
    responseData.preemptiveConnectionCount =
        m_settings.listeningPeer().recommendedPreemptiveConnectionCount;
    responseData.keepAliveOptions = m_settings.listeningPeer().tcpKeepAlive;

    serializeToHeaders(&response->headers, responseData);
    //--

    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Connection", "close");
}

template<typename RequestSpecificData>
void GetPostTunnelProcessor<RequestSpecificData>::openUpTunnel(
    network::http::HttpServerConnection* const connection,
    RequestSpecificData requestData,
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
        TunnelContext{std::move(requestData), requestPath, std::move(httpPipe)});

    insertionResult.first->second.connection->setMessageHandler(
        std::bind(&GetPostTunnelProcessor::onMessage, this, httpPipePtr, _1));
    insertionResult.first->second.connection->startReadingConnection();
}

template<typename RequestSpecificData>
void GetPostTunnelProcessor<RequestSpecificData>::onMessage(
    network::http::AsyncMessagePipeline* tunnel,
    network::http::Message message)
{
    QnMutexLocker lock(&m_mutex);

    auto tunnelIter = m_tunnelsInProgress.find(tunnel);
    if (tunnelIter == m_tunnelsInProgress.end())
        return; //< GetPostTunnelProcessor is being destroyed.

    if (!validateOpenUpChannelMessage(tunnelIter->second, message))
    {
        NX_DEBUG(this, lm("Invalid up channel. Url %1")
            .args(tunnelIter->second.urlPath));
        closeConnection(lock, SystemError::invalidData, tunnelIter->first);
        return;
    }

    auto tunnelContext = std::move(tunnelIter->second);
    m_tunnelsInProgress.erase(tunnelIter);

    NX_VERBOSE(this, lm("Successfully opened GET/POST tunnel. Url %1")
        .args(tunnelContext.urlPath));

    onTunnelCreated(
        std::move(tunnelContext.requestData),
        tunnelContext.connection->takeSocket());
}

template<typename RequestSpecificData>
bool GetPostTunnelProcessor<RequestSpecificData>::validateOpenUpChannelMessage(
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

template<typename RequestSpecificData>
void GetPostTunnelProcessor<RequestSpecificData>::closeConnection(
    SystemError::ErrorCode closeReason,
    network::http::AsyncMessagePipeline* connection)
{
    QnMutexLocker lock(&m_mutex);
    closeConnection(lock, closeReason, connection);
}

template<typename RequestSpecificData>
void GetPostTunnelProcessor<RequestSpecificData>::closeConnection(
    const QnMutexLockerBase& /*lock*/,
    SystemError::ErrorCode /*closeReason*/,
    network::http::AsyncMessagePipeline* connection)
{
    m_tunnelsInProgress.erase(connection);
}

//-------------------------------------------------------------------------------------------------

template class GetPostTunnelProcessor<std::string>;

GetPostServerTunnelProcessor::GetPostServerTunnelProcessor(
    const conf::Settings& settings,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    base_type(settings),
    m_listeningPeerPool(listeningPeerPool)
{
}

void GetPostServerTunnelProcessor::onTunnelCreated(
    std::string listeningPeerName,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_listeningPeerPool->addConnection(
        listeningPeerName,
        std::move(connection));
}

//-------------------------------------------------------------------------------------------------

template class GetPostTunnelProcessor<
    controller::AbstractConnectSessionManager::StartRelayingFunc>;

GetPostClientTunnelProcessor::GetPostClientTunnelProcessor(
    const conf::Settings& settings)
    :
    base_type(settings)
{
}

void GetPostClientTunnelProcessor::onTunnelCreated(
    controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    startRelayingFunc(std::move(connection));
}

} // namespace nx::cloud::relay::view
