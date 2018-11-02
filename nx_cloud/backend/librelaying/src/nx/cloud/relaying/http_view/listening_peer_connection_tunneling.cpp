#include "listening_peer_connection_tunneling.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx::cloud::relaying {

ListeningPeerConnectionTunnelingServer::ListeningPeerConnectionTunnelingServer(
    relaying::AbstractListeningPeerManager* listeningPeerManager,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_listeningPeerManager(listeningPeerManager),
    m_listeningPeerPool(listeningPeerPool),
    m_tunnelingServer(
        [this](auto&&... args) { saveNewTunnel(std::move(args)...); },
        this)
{
}

void ListeningPeerConnectionTunnelingServer::registerHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    auto& compatibilityTunnelServer = m_tunnelingServer.addTunnelServer<
        nx::network::http::tunneling::detail::ConnectionUpgradeTunnelServer>();

    compatibilityTunnelServer.setProtocolName(
        relay::api::kRelayProtocolName);
    compatibilityTunnelServer.setRequestPath(
        relay::api::kServerIncomingConnectionsPath);
    compatibilityTunnelServer.setUpgradeRequestMethod(
        network::http::Method::post);

    m_tunnelingServer.registerRequestHandlers(
        basePath,
        messageDispatcher);
}

void ListeningPeerConnectionTunnelingServer::authorize(
    const nx::network::http::RequestContext* requestContext,
    CompletionHandler completionHandler)
{
    const auto serverId = 
        requestContext->requestPathParams.getByName(relay::api::kServerIdName);
    if (serverId.empty())
    {
        return completionHandler(
            nx::network::http::StatusCode::badRequest,
            std::string());
    }

    relay::api::BeginListeningRequest beginListeningRequest{serverId};

    m_listeningPeerManager->beginListening(
        std::move(beginListeningRequest),
        [this, completionHandler = std::move(completionHandler),
            peerName = beginListeningRequest.peerName,
            response = requestContext->response](
                auto... args) mutable
        {
            onBeginListeningCompletion(
                std::move(completionHandler),
                peerName,
                response,
                std::move(args)...);
        });
}

void ListeningPeerConnectionTunnelingServer::onBeginListeningCompletion(
    CompletionHandler completionHandler,
    const std::string& peerName,
    network::http::Response* httpResponse,
    relay::api::ResultCode resultCode,
    relay::api::BeginListeningResponse response,
    nx::network::http::ConnectionEvents /*connectionEvents*/)
{
    if (resultCode != relay::api::ResultCode::ok)
    {
        completionHandler(nx::network::http::StatusCode::forbidden, peerName);
        return;
    }

    serializeToHeaders(&httpResponse->headers, response);
    completionHandler(nx::network::http::StatusCode::ok, peerName);
}

void ListeningPeerConnectionTunnelingServer::saveNewTunnel(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    const std::string& listeningPeerName)
{
    m_listeningPeerPool->addConnection(
        listeningPeerName,
        std::move(connection));
}

} // namespace nx::cloud::relaying
