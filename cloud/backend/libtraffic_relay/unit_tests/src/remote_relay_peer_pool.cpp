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

cf::future<std::string /*relay hostname/ip*/> RemoteRelayPeerPool::findRelayByDomain(
    const std::string& domainName) const
{
    cf::promise<std::string> p;
    auto f = p.get_future();
    m_asyncCallProvider.post(
        [this, domainName, p = std::move(p)]() mutable
        {
            p.set_value(m_peerPool->findRelay(domainName));
        });
    return f;
}

cf::future<bool> RemoteRelayPeerPool::addPeer(const std::string& domainName)
{
    m_peerPool->add(domainName, m_endpoint);
    return cf::make_ready_future(true);
}

cf::future<bool> RemoteRelayPeerPool::removePeer(const std::string& domainName)
{
    m_peerPool->remove(domainName);
    return cf::make_ready_future(true);
}

void RemoteRelayPeerPool::setNodeId(const std::string& nodeId)
{
    m_endpoint = nodeId;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
