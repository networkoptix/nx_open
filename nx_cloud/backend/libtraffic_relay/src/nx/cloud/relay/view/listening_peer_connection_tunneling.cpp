#include "listening_peer_connection_tunneling.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx::cloud::relay::view {

ListeningPeerConnectionTunnelingServer::ListeningPeerConnectionTunnelingServer(
    relaying::AbstractListeningPeerManager* listeningPeerManager,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_listeningPeerManager(listeningPeerManager),
    m_listeningPeerPool(listeningPeerPool),
    m_tunnelingServer(
        std::bind(&ListeningPeerConnectionTunnelingServer::saveNewTunnel, this,
            std::placeholders::_1, std::placeholders::_2),
        this)
{
}

void ListeningPeerConnectionTunnelingServer::registerHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_tunnelingServer.connectionUpgradeServer().setProtocolName(
        relay::api::kRelayProtocolName);
    m_tunnelingServer.connectionUpgradeServer().setRequestPath(
        relay::api::kServerIncomingConnectionsPath);
    m_tunnelingServer.connectionUpgradeServer().setUpgradeRequestMethod(
        network::http::Method::post);

    m_tunnelingServer.registerRequestHandlers(
        basePath,
        messageDispatcher);
}

void ListeningPeerConnectionTunnelingServer::authorize(
    const nx::network::http::RequestContext* requestContext,
    CompletionHandler completionHandler)
{
    using namespace std::placeholders;

    if (requestContext->requestPathParams.empty())
    {
        return completionHandler(
            nx::network::http::StatusCode::badRequest,
            std::string());
    }

    api::BeginListeningRequest beginListeningRequest{
        requestContext->requestPathParams[0].toStdString()};

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
    if (resultCode != api::ResultCode::ok)
    {
        completionHandler(nx::network::http::StatusCode::forbidden, peerName);
        return;
    }

    serializeToHeaders(&httpResponse->headers, response);
    completionHandler(nx::network::http::StatusCode::ok, peerName);
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
