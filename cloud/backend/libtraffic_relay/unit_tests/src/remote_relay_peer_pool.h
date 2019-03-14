#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relay/model/remote_relay_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class ListeningPeerPool
{
public:
    void add(const std::string& peerName, const std::string& relayEndpoint);
    void remove(const std::string& peerName);
    std::string findRelay(const std::string& peerName) const;

private:
    mutable QnMutex m_mutex;
    std::map<std::string /*listening peer name*/, std::string /*relay endpoint*/>
        m_peerNameToRelayEndpoint;
};

//-------------------------------------------------------------------------------------------------

class RemoteRelayPeerPool:
    public model::AbstractRemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool(ListeningPeerPool* peerPool);
    ~RemoteRelayPeerPool();

    virtual bool connectToDb() override;

    virtual bool isConnected() const override;

    virtual cf::future<std::string /*relay hostname/ip*/> findRelayByDomain(
        const std::string& domainName) const override;

    virtual cf::future<bool> addPeer(const std::string& domainName) override;

    virtual cf::future<bool> removePeer(const std::string& domainName) override;

    virtual void setNodeId(const std::string& nodeId) override;

private:
    mutable nx::network::aio::BasicPollable m_asyncCallProvider;
    ListeningPeerPool* m_peerPool;
    std::string m_endpoint;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
