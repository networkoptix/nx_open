#include "listening_peer_connection_tunneling.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>

namespace nx::cloud::relay::view {

ListeningPeerConnectionTunnelingServer::ListeningPeerConnectionTunnelingServer(
    const conf::Settings& settings,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool)
{
}

void ListeningPeerConnectionTunnelingServer::registerHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_tunnelingServer = std::make_unique<TunnelingServer>(
        basePath,
        messageDispatcher,
        std::bind(&ListeningPeerConnectionTunnelingServer::saveNewTunnel, this,
            std::placeholders::_1, std::placeholders::_2),
        this);
}

void ListeningPeerConnectionTunnelingServer::authorize(
    const nx::network::http::RequestContext* requestContext,
    CompletionHandler completionHandler)
{
    if (requestContext->requestPathParams.empty())
    {
        return completionHandler(
            nx::network::http::StatusCode::badRequest,
            std::string());
    }

    api::BeginListeningResponse responseData;
    responseData.preemptiveConnectionCount =
        m_settings.listeningPeer().recommendedPreemptiveConnectionCount;
    responseData.keepAliveOptions = m_settings.listeningPeer().tcpKeepAlive;

    serializeToHeaders(&requestContext->response->headers, responseData);

    completionHandler(
        nx::network::http::StatusCode::ok,
        requestContext->requestPathParams[0].toStdString());
}

void ListeningPeerConnectionTunnelingServer::saveNewTunnel(
    const std::string& listeningPeerName,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_listeningPeerPool->addConnection(
        listeningPeerName,
        std::move(connection));
}

} // namespace nx::cloud::relay::view
