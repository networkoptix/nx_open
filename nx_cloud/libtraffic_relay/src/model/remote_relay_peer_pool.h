#pragma once

#include <memory>
#include <string>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx {

namespace cassandra {
class AsyncConnection;
}

namespace cloud {
namespace relay {
namespace model {

class RemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool();

    cf::future<std::string> findRelayByDomain(const std::string& domainName) const;
    bool addPeer(const std::string& domainName, const std::string& peerName);

private:
    std::unique_ptr<cassandra::AsyncConnection> m_cassConnection;
    bool m_dbReady = false;

    void prepareDbStructure();
    std::string whereStringForFind(const std::string& domainName);
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
