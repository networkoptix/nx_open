#pragma once

#include <memory>
#include <string>
#include <nx/utils/thread/cf/cfuture.h>

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

    cf::future<std::string> findRelayByDomain(const std::string& domainName) const;
    cf::future<bool> addPeer(const std::string& relayName, const std::string& domainName);

protected:
    cassandra::AsyncConnection* getConnection();

private:
    std::unique_ptr<cassandra::AsyncConnection> m_cassConnection;
    bool m_dbReady = false;

    void prepareDbStructure();
    std::string whereStringForFind(const std::string& domainName) const;
    bool bindInsertParameters(
        cassandra::Query* query,
        const std::string& relayName,
        const std::string& domainName) const;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
