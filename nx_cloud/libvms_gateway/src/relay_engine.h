#pragma once

#include <memory>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/cloud/relay/controller/listening_peer_manager.h>
#include <nx/cloud/relay/model/listening_peer_pool.h>
#include <nx/cloud/relay/settings.h>

namespace nx {
namespace cloud {
namespace gateway {

class RelayEngine
{
public:
    RelayEngine(
        const relay::conf::ListeningPeer& settings,
        nx_http::server::rest::MessageDispatcher* httpMessageDispatcher);

    relay::model::ListeningPeerPool& listeningPeerPool();

private:
    relay::model::ListeningPeerPool m_listeningPeerPool;
    std::unique_ptr<relay::controller::AbstractListeningPeerManager> m_listeningPeerManager;

    void registerApiHandlers(
        nx_http::server::rest::MessageDispatcher* httpMessageDispatcher);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
