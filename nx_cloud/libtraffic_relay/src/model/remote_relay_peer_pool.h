#pragma once

#include <memory>
#include <string>
#include <utility>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

namespace nx {

namespace cassandra {
class AsyncConnection;
class Query;
}

namespace cloud {
namespace relay {
namespace model {

class RemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool(const char* cassandraHost);
    ~RemoteRelayPeerPool();

    cf::future<std::string> findRelayByDomain(const std::string& domainName) const;
    cf::future<bool> addPeer(const std::string& domainName, const std::string& relayHost);
    cf::future<bool> removePeer(const std::string& domainName, const std::string& relayHost);

protected:
    cassandra::AsyncConnection* getConnection();

private:
    std::unique_ptr<cassandra::AsyncConnection> m_cassConnection;
    bool m_dbReady = false;
    mutable std::string m_hostId;
    mutable QnMutex m_mutex;

    void prepareDbStructure();
    std::string whereStringForFind(const std::string& domainName) const;
    bool bindUpdateParameters(
        cassandra::Query* query,
        const std::string& domainName,
        const std::string& relayHost) const;
    cf::future<int> getLocalHostId() const;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
