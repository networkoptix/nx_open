#pragma once

#include <memory>
#include <nx/utils/move_only_func.h>
#include "client_session_pool.h"
#include "listening_peer_pool.h"
#include "abstract_remote_relay_peer_pool.h"

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

    model::AbstractRemoteRelayPeerPool& remoteRelayPeerPool();
    const model::AbstractRemoteRelayPeerPool& remoteRelayPeerPool() const;

private:
    model::ClientSessionPool m_clientSessionPool;
    model::ListeningPeerPool m_listeningPeerPool;
    std::unique_ptr<model::AbstractRemoteRelayPeerPool> m_remoteRelayPeerPool;
};

namespace model {

class RemoteRelayPeerPoolFactory
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<
        std::unique_ptr<model::AbstractRemoteRelayPeerPool>(const conf::Settings&)>;
    static std::unique_ptr<model::AbstractRemoteRelayPeerPool> create(
        const conf::Settings& settings);
    static void setFactoryFunc(FactoryFunc func);
};

} // namespace model

} // namespace relay
} // namespace cloud
} // namespace nx
