#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

#include "abstract_remote_relay_peer_pool.h"
#include "alias_manager.h"
#include "client_session_pool.h"
#include "remote_relay_peer_pool_aio_wrapper.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }

class Model
{
public:
    Model(const conf::Settings& settings);
    ~Model();

    bool doMandatoryInitialization();

    model::ClientSessionPool& clientSessionPool();
    const model::ClientSessionPool& clientSessionPool() const;

    relaying::ListeningPeerPool& listeningPeerPool();
    const relaying::ListeningPeerPool& listeningPeerPool() const;

    model::AbstractRemoteRelayPeerPool& remoteRelayPeerPool();
    const model::AbstractRemoteRelayPeerPool& remoteRelayPeerPool() const;

    model::RemoteRelayPeerPoolAioWrapper& remoteRelayPeerPoolAioWrapper();

    model::AliasManager& aliasManager();
    const model::AliasManager& aliasManager() const;

private:
    const conf::Settings& m_settings;
    model::ClientSessionPool m_clientSessionPool;
    relaying::ListeningPeerPool m_listeningPeerPool;
    std::unique_ptr<model::AbstractRemoteRelayPeerPool> m_remoteRelayPeerPool;
    model::RemoteRelayPeerPoolAioWrapper m_remoteRelayPeerPoolAioWrapper;
    model::AliasManager m_aliasManager;
    std::vector<nx::utils::SubscriptionId> m_listeningPeerPoolSubscriptions;

    void subscribeForPeerConnected(nx::utils::SubscriptionId* subscriptionId);
    void subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId);
};

} // namespace relay
} // namespace cloud
} // namespace nx
