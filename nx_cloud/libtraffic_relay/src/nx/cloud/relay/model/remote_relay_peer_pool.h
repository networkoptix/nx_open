#pragma once

#include <memory>
#include <string>
#include <utility>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_remote_relay_peer_pool.h"

namespace nx {

namespace cassandra {
class AbstractAsyncConnection;
class Query;
} // namespace cassandra

namespace cloud {
namespace relay {

namespace conf { class Settings; }

namespace model {

class RemoteRelayPeerPool:
    public AbstractRemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool(const conf::Settings& settings);
    ~RemoteRelayPeerPool();

    virtual bool connectToDb() override;
    virtual bool isConnected() const override;
    /**
     * @return cf::future<Relay instance endpoint that has peer domainName listening>.
     */
    virtual cf::future<std::string> findRelayByDomain(
        const std::string& domainName) const override;
    virtual cf::future<bool> addPeer(const std::string& domainName) override;
    virtual cf::future<bool> removePeer(const std::string& domainName) override;
    virtual void setNodeId(const std::string& nodeId) override;

protected:
    cassandra::AbstractAsyncConnection* getConnection();

private:
    enum class UpdateType
    {
        add,
        remove
    };

    const conf::Settings& m_settings;
    std::unique_ptr<cassandra::AbstractAsyncConnection> m_cassConnection;
    bool m_dbReady = false;
    mutable QnMutex m_mutex;
    std::string m_nodeId;

    void prepareDbStructure();
    std::string whereStringForFind(const std::string& domainName) const;
    bool bindUpdateParameters(
        cassandra::Query* query,
        const std::string& domainName,
        UpdateType updateType) const;
};

//-------------------------------------------------------------------------------------------------

using RemoteRelayPeerPoolFactoryFunc =
    std::unique_ptr<model::AbstractRemoteRelayPeerPool>(
        const conf::Settings&);

class RemoteRelayPeerPoolFactory:
    public nx::utils::BasicFactory<RemoteRelayPeerPoolFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<RemoteRelayPeerPoolFactoryFunc>;

public:
    RemoteRelayPeerPoolFactory();

    static RemoteRelayPeerPoolFactory& instance();

private:
    std::unique_ptr<model::AbstractRemoteRelayPeerPool> defaultFactory(
        const conf::Settings& settings);
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
