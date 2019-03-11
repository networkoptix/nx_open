#include "remote_relay_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

void ListeningPeerPool::add(
    const std::string& peerName,
    const std::string& relayEndpoint)
{
    QnMutexLocker lock(&m_mutex);

    m_peerNameToRelayEndpoint.emplace(peerName, relayEndpoint);
}

void ListeningPeerPool::remove(const std::string& peerName)
{
    QnMutexLocker lock(&m_mutex);

    m_peerNameToRelayEndpoint.erase(peerName);
}

std::string ListeningPeerPool::findRelay(const std::string& peerName) const
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_peerNameToRelayEndpoint.find(peerName);
    if (it != m_peerNameToRelayEndpoint.end())
        return it->second;
    return std::string();
}

//-------------------------------------------------------------------------------------------------

RemoteRelayPeerPool::RemoteRelayPeerPool(ListeningPeerPool* peerPool):
    m_peerPool(peerPool)
{
}

RemoteRelayPeerPool::~RemoteRelayPeerPool()
{
    m_asyncCallProvider.pleaseStopSync();
}

bool RemoteRelayPeerPool::connectToDb()
{
    return true;
}

bool RemoteRelayPeerPool::isConnected() const
{
    return true;
}

void RemoteRelayPeerPool::findRelayByDomain(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(std::string /*relay hostname/ip*/)> handler) const
{
    m_asyncCallProvider.post(
        [this, domainName, handler = std::move(handler)]() mutable
        {
            handler(m_peerPool->findRelay(domainName));
        });
}

void RemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_peerPool->add(domainName, m_endpoint);
    return handler(true);
}

void RemoteRelayPeerPool::removePeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_peerPool->remove(domainName);
    return handler(true);
}

void RemoteRelayPeerPool::setNodeId(const std::string& nodeId)
{
    m_endpoint = nodeId;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
