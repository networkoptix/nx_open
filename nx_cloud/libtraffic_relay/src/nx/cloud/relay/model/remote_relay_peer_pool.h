#pragma once

#include <memory>
#include <string>
#include <utility>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>
#include "abstract_remote_relay_peer_pool.h"

namespace nx {

namespace cassandra {
class AbstractAsyncConnection;
class Query;
}

namespace cloud {
namespace relay {

namespace conf { class Settings; }

namespace model {

class RemoteRelayPeerPool: public AbstractRemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool(const conf::Settings& settings);
    ~RemoteRelayPeerPool();

    virtual bool connectToDb() override;

    virtual cf::future<std::string> findRelayByDomain(
        const std::string& domainName) const override;

    virtual cf::future<bool> addPeer(
        const std::string& domainName,
        const std::string& relayHost) override;

    virtual cf::future<bool> removePeer(const std::string& domainName) override;

    bool isConnectedToDb() const;

protected:
    cassandra::AbstractAsyncConnection* getConnection();

private:
    const conf::Settings& m_settings;
    std::unique_ptr<cassandra::AbstractAsyncConnection> m_cassConnection;
    bool m_dbReady = false;
    mutable std::string m_hostId;
    mutable QnMutex m_mutex;

    void prepareDbStructure();
    std::string whereStringForFind(const std::string& domainName) const;
    bool bindUpdateParameters(
        cassandra::Query* query,
        const std::string& domainName,
        const boost::optional<std::string>& relayHost = boost::none) const;
    cf::future<int> getLocalHostId() const;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
