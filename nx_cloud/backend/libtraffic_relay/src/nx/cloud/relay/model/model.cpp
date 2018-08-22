#include "model.h"

#include <nx/utils/log/log.h>

#include "remote_relay_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

Model::Model(const conf::Settings& settings):
    m_settings(settings),
    m_clientSessionPool(settings),
    m_listeningPeerPool(settings.listeningPeer()),
    m_remoteRelayPeerPool(
        model::RemoteRelayPeerPoolFactory::instance().create(settings)),
    m_remoteRelayPeerPoolAioWrapper(*m_remoteRelayPeerPool),
    m_aliasManager(settings.proxy().unusedAliasExpirationPeriod)
{
    if (m_remoteRelayPeerPool)
    {
        nx::utils::SubscriptionId subscriptionId;
        subscribeForPeerConnected(&subscriptionId);
        m_listeningPeerPoolSubscriptions.push_back(subscriptionId);

        subscribeForPeerDisconnected(&subscriptionId);
        m_listeningPeerPoolSubscriptions.push_back(subscriptionId);
    }
}

Model::~Model()
{
    for (const auto& subscriptionId: m_listeningPeerPoolSubscriptions)
    {
        m_listeningPeerPool.peerConnectedSubscription()
            .removeSubscription(subscriptionId);
    }
}

bool Model::doMandatoryInitialization()
{
    // TODO: #ak Refactor here. E.g., if DB connection parameters are specified,
    // we can create some dummy RemoteRelayPeerPool so that we keep real RemoteRelayPeerPool
    // implementation clear (free of DB needed/not needed checks).
    if (m_settings.cassandraConnection().host.empty())
        return true;
    return m_remoteRelayPeerPool->connectToDb();
}

model::ClientSessionPool& Model::clientSessionPool()
{
    return m_clientSessionPool;
}

const model::ClientSessionPool& Model::clientSessionPool() const
{
    return m_clientSessionPool;
}

relaying::ListeningPeerPool& Model::listeningPeerPool()
{
    return m_listeningPeerPool;
}

const relaying::ListeningPeerPool& Model::listeningPeerPool() const
{
    return m_listeningPeerPool;
}

model::AbstractRemoteRelayPeerPool& Model::remoteRelayPeerPool()
{
    return *m_remoteRelayPeerPool;
}

const model::AbstractRemoteRelayPeerPool& Model::remoteRelayPeerPool() const
{
    return *m_remoteRelayPeerPool;
}

model::RemoteRelayPeerPoolAioWrapper& Model::remoteRelayPeerPoolAioWrapper()
{
    return m_remoteRelayPeerPoolAioWrapper;
}

model::AliasManager& Model::aliasManager()
{
    return m_aliasManager;
}

const model::AliasManager& Model::aliasManager() const
{
    return m_aliasManager;
}

void Model::subscribeForPeerConnected(nx::utils::SubscriptionId* subscriptionId)
{
    m_listeningPeerPool.peerConnectedSubscription().subscribe(
        [this](std::string peer)
        {
            m_remoteRelayPeerPool->addPeer(peer)
                .then(
                    [this, peer](cf::future<bool> addPeerFuture)
                    {
                        if (addPeerFuture.get())
                        {
                            NX_VERBOSE(this, lm("Successfully added peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }
                        else
                        {
                            NX_VERBOSE(this, lm("Failed to add peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }

                        return cf::unit();
                    });
        },
        subscriptionId);
}

void Model::subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId)
{
    m_listeningPeerPool.peerDisconnectedSubscription().subscribe(
        [this](std::string peer)
        {
            m_remoteRelayPeerPool->removePeer(peer)
                .then(
                    [this, peer](cf::future<bool> removePeerFuture)
                    {
                        if (removePeerFuture.get())
                        {
                            NX_VERBOSE(this, lm("Successfully removed peer %1 from RemoteRelayPool")
                                .arg(peer));
                        }
                        else
                        {
                            NX_VERBOSE(this, lm("Failed to remove peer %1 from RemoteRelayPool")
                                .arg(peer));
                        }

                        return cf::unit();
                    });
        },
        subscriptionId);
}

} // namespace relay
} // namespace cloud
} // namespace nx
