#include "model.h"
#include "remote_relay_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {

Model::Model(const conf::Settings& settings):
    m_clientSessionPool(settings),
    m_listeningPeerPool(settings),
    m_remoteRelayPeerPool(model::RemoteRelayPeerPoolFactory::create(settings))
{
}

model::ClientSessionPool& Model::clientSessionPool()
{
    return m_clientSessionPool;
}

const model::ClientSessionPool& Model::clientSessionPool() const
{
    return m_clientSessionPool;
}

model::ListeningPeerPool& Model::listeningPeerPool()
{
    return m_listeningPeerPool;
}

const model::ListeningPeerPool& Model::listeningPeerPool() const
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

namespace model {

static RemoteRelayPeerPoolFactory::FactoryFunc remoteRelayPeerPoolFactoryFunc;

std::unique_ptr<model::AbstractRemoteRelayPeerPool> RemoteRelayPeerPoolFactory::create(
    const conf::Settings& settings)
{
    if (remoteRelayPeerPoolFactoryFunc)
        return remoteRelayPeerPoolFactoryFunc(settings);

    return std::make_unique<model::RemoteRelayPeerPool>(settings);
}

void RemoteRelayPeerPoolFactory::setFactoryFunc(FactoryFunc func)
{
    remoteRelayPeerPoolFactoryFunc.swap(func);
}

} // namespace model

} // namespace relay
} // namespace cloud
} // namespace nx
