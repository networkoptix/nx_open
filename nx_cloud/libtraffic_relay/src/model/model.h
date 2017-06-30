#pragma once

#include "client_session_pool.h"
#include "listening_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }

class Model
{
public:
    Model(const conf::Settings& settings);

    model::ClientSessionPool& clientSessionPool();
    const model::ClientSessionPool& clientSessionPool() const;

    model::ListeningPeerPool& listeningPeerPool();
    const model::ListeningPeerPool& listeningPeerPool() const;

private:
    model::ClientSessionPool m_clientSessionPool;
    model::ListeningPeerPool m_listeningPeerPool;
};

} // namespace relay
} // namespace cloud
} // namespace nx
