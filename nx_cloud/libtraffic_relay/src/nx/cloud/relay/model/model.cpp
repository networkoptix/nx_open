#include "model.h"

#include "remote_relay_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

Model::Model(const conf::Settings& settings):
    m_settings(settings),
    m_clientSessionPool(settings),
    m_listeningPeerPool(settings.listeningPeer()),
    m_remoteRelayPeerPool(model::RemoteRelayPeerPoolFactory::create(settings))
{
}

bool Model::doMandatoryInitialization()
{
    // TODO: #ak Refactor here. E.g., if DB connection parameters are specified, 
    // we can create some dummy RemoteRelayPeerPool so that we keep real RemoteRelayPeerPool 
    // implementation clear (free of DB needed/not needed checks).
    if (m_settings.cassandraConnection().hostName.empty())
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

} // namespace relay
} // namespace cloud
} // namespace nx
