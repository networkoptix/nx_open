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
        relay::api::kRelayProtocol);
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
            relay::api::BeginListeningRequest());
    }

    relay::api::BeginListeningRequest beginListeningRequest{serverId, ""};

    if (auto nxUpgrade = requestContext->request.headers.find(relay::api::kNxProtocolHeader);
        nxUpgrade != requestContext->request.headers.end())
    {
        network::http::MimeProtoVersion protocol;
        if (protocol.parse(nxUpgrade->second))
            beginListeningRequest.protocolVersion = protocol.version.toStdString();
    }

    m_listeningPeerManager->beginListening(
        std::move(beginListeningRequest),
        [this, completionHandler = std::move(completionHandler),
            beginListeningRequest,
            response = requestContext->response](
                auto... args) mutable
        {
            onBeginListeningCompletion(
                std::move(completionHandler),
                std::move(beginListeningRequest),
                response,
                std::move(args)...);
        });
}

void ListeningPeerConnectionTunnelingServer::onBeginListeningCompletion(
    CompletionHandler completionHandler,
    relay::api::BeginListeningRequest request,
    network::http::Response* httpResponse,
    relay::api::ResultCode resultCode,
    relay::api::BeginListeningResponse response,
    nx::network::http::ConnectionEvents /*connectionEvents*/)
{
    if (resultCode != relay::api::ResultCode::ok)
    {
        completionHandler(nx::network::http::StatusCode::forbidden, std::move(request));
        return;
    }

    serializeToHeaders(&httpResponse->headers, response);
    completionHandler(nx::network::http::StatusCode::ok, std::move(request));
}

void ListeningPeerConnectionTunnelingServer::saveNewTunnel(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    const relay::api::BeginListeningRequest& request)
{
    m_listeningPeerPool->addConnection(
        request.peerName,
        request.protocolVersion,
        std::move(connection));
}

} // namespace nx::cloud::relaying
