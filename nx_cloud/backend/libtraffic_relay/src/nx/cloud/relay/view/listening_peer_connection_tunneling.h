#pragma once

#include <nx/network/http/tunneling/server.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "../settings.h"

namespace nx::cloud::relay::view {

class ListeningPeerConnectionTunnelingServer:
    public nx::network::http::tunneling::TunnelAuthorizer<std::string>
{
public:
    ListeningPeerConnectionTunnelingServer(
        const conf::Settings& settings,
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

    const conf::Settings& m_settings;
    relaying::AbstractListeningPeerPool* m_listeningPeerPool = nullptr;
    std::unique_ptr<TunnelingServer> m_tunnelingServer;

    void saveNewTunnel(
        const std::string& listeningPeerName,
        std::unique_ptr<network::AbstractStreamSocket> connection);
};

} // namespace nx::cloud::relay::view
