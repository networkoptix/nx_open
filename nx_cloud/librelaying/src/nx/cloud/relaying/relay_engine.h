#pragma once

#include <memory>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "listening_peer_manager.h"
#include "listening_peer_pool.h"
#include "settings.h"

namespace nx {
namespace cloud {
namespace relaying {

class NX_RELAYING_API RelayEngine
{
public:
    RelayEngine(
        const Settings& settings,
        nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher);

    ListeningPeerPool& listeningPeerPool();

private:
    ListeningPeerPool m_listeningPeerPool;
    std::unique_ptr<AbstractListeningPeerManager> m_listeningPeerManager;

    void registerApiHandlers(
        nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher);
};

} // namespace relaying
} // namespace cloud
} // namespace nx
