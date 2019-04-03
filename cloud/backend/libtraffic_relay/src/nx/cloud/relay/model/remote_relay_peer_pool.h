#pragma once

#include "abstract_remote_relay_peer_pool.h"

#include <memory>
#include <string>
#include <utility>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

#include <nx/clusterdb/map/embedded_database.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "nx/cloud/relay/settings.h"

namespace nx {

namespace cloud::relay::model {

class RemoteRelayPeerPool:
    public AbstractRemoteRelayPeerPool
{
public:
    RemoteRelayPeerPool(const conf::Settings& settings);
    virtual ~RemoteRelayPeerPool();

    /**
      * NOTE: Can be blocking. But, must return eventually.
      */
    virtual bool connectToDb() override;

    virtual bool isConnected() const override;

    /**
     * @return empty string in case of any error.
     * @param domainName "serverId.systemId" or just "systemId"
     */
    virtual void findRelayByDomain(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(std::string /*relay hostname/ip*/)> handler) const override;

    /**
     * Associates domainName with the url given to setPublicUrl().
     */
    virtual void addPeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

    virtual void removePeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) override;

    virtual void setPublicUrl(const nx::utils::Url& publicUrl) override;

    virtual void registerHttpApi(
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher) override;

private:
    void startDiscovery();

private:
    const conf::ListeningPeerDb& m_settings;
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_queryExecutor;
    std::unique_ptr<nx::clusterdb::map::EmbeddedDatabase> m_map;

    std::string m_baseApiPath;
    nx::network::http::server::rest::MessageDispatcher* m_messageDispatcher = nullptr;
    bool m_httpApiRegistered = false;

    std::string m_domainName;
    nx::utils::Url m_syncEngineUrl;
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

} // namespace cloud::relay::model
} // namespace nx
