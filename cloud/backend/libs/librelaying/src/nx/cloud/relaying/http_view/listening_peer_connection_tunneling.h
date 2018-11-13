#pragma once

#include <nx/network/http/tunneling/server.h>

#include "../listening_peer_pool.h"
#include "../listening_peer_manager.h"

namespace nx::cloud::relaying {

class NX_RELAYING_API ListeningPeerConnectionTunnelingServer:
    public nx::network::http::tunneling::TunnelAuthorizer<std::string>
{
public:
    ListeningPeerConnectionTunnelingServer(
        relaying::AbstractListeningPeerManager* listeningPeerManager,
        relaying::AbstractListeningPeerPool* listeningPeerPool);

    void registerHandlers(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher);

protected:
    virtual void authorize(
        const nx::network::http::RequestContext* requestContext,
        CompletionHandler completionHandler) override;

private:
    using TunnelingServer = 
        nx::network::http::tunneling::Server<std::string /*listeningPeerName*/>;

    relaying::AbstractListeningPeerManager* m_listeningPeerManager = nullptr;
    relaying::AbstractListeningPeerPool* m_listeningPeerPool = nullptr;
    TunnelingServer m_tunnelingServer;

    void onBeginListeningCompletion(
        CompletionHandler completionHandler,
        const std::string& peerName,
        network::http::Response* httpResponse,
        relay::api::ResultCode resultCode,
        relay::api::BeginListeningResponse response,
        nx::network::http::ConnectionEvents connectionEvents);

    void saveNewTunnel(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        const std::string& listeningPeerName);
};

} // namespace nx::cloud::relaying
