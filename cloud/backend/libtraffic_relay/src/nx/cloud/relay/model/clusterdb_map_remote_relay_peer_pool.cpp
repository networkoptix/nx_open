#include "clusterdb_map_remote_relay_peer_pool.h"

#include <nx/utils/std/algorithm.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/clusterdb/engine/http/http_paths.h>

#include "nx/cloud/relay/settings.h"

namespace nx::cloud::relay::model {

namespace {

static std::string toLowerReversed(std::string domainName)
{
    nx::utils::to_lower(&domainName);
    return nx::utils::reverseWords(domainName, ".");
}

} //namespace

ClusterDbMapRemoteRelayPeerPool::ClusterDbMapRemoteRelayPeerPool(const conf::Settings& settings):
    m_settings(settings.clusterDbMap()),
    m_baseApiPath(nx::clusterdb::engine::kBaseSynchronizationPath)
{
    // Try to connect immediately
    connectToDb();
}

ClusterDbMapRemoteRelayPeerPool::~ClusterDbMapRemoteRelayPeerPool()
{
    if (m_map)
        m_map->synchronizationEngine().pleaseStopSync();
}

bool ClusterDbMapRemoteRelayPeerPool::connectToDb()
{
    if (isConnected())
        return true;

    try
    {
        auto queryExecutor =
            std::make_unique<nx::sql::AsyncSqlQueryExecutor>(
                m_settings.sql);

        // Throws exception on error.
        auto map =
            std::make_unique<nx::clusterdb::map::EmbeddedDatabase>(
                m_settings.map, queryExecutor.get());

        m_queryExecutor = std::move(queryExecutor);
        m_map = std::move(map);

        if (m_messageDispatcher)
            registerHttpApi(m_messageDispatcher);

        startDiscovery();
    }
    catch (const std::exception& e)
    {
        NX_ERROR(
            this,
            "Failed to initialize database, traffic relay will not function properly: %1",
            e.what());
    }

    return isConnected();
}

bool ClusterDbMapRemoteRelayPeerPool::isConnected() const
{
    return m_map != nullptr;
}

void ClusterDbMapRemoteRelayPeerPool::findRelayByDomain(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(std::string)> handler) const
{
    using namespace nx::clusterdb::map;

    if (!isConnected())
        return handler(std::string());

    m_map->database().dataManager().getRangeWithPrefix(
        toLowerReversed(domainName),
        [this, domainName, handler = std::move(handler)](
            ResultCode result, std::map<std::string, std::string> map)
        {
            if (result != ResultCode::ok)
            {
                NX_WARNING(
                    this,
                    "getRangeWithPrefix returned error: %1 for key: %2",
                    toString(result), domainName);
                return handler(std::string());
            }

            NX_VERBOSE(
                this,
                "getRangeWithPrefix returned ResultCode: %1 and result set: %2 for domainName: %3",
                    toString(result), containerString(map), domainName);

            if (map.empty())
                handler(std::string());
            else
                handler(map.begin()->second);
        });
}

void ClusterDbMapRemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    using namespace nx::clusterdb::map;

    if (!isConnected())
        return handler(false);

    m_map->database().dataManager().insertOrUpdate(
        toLowerReversed(domainName),
        m_domainName,
        [this, domainName, handler = std::move(handler)](ResultCode result)
        {
            if (result != ResultCode::ok)
            {
                NX_WARNING(
                    this,
                    "insertOrUpdate returned error: %1 for args: key: %2, value: %3",
                    toString(result), domainName, m_domainName);
            }
            handler(result == ResultCode::ok);
        });
}

void ClusterDbMapRemoteRelayPeerPool::removePeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    using namespace nx::clusterdb::map;

    if (!isConnected())
        return handler(false);

    m_map->database().dataManager().remove(
        toLowerReversed(domainName),
        [this, domainName, handler = std::move(handler)](ResultCode result)
        {
            if (result != ResultCode::ok)
            {
                NX_VERBOSE(
                    this,
                    "remove returned error: %1, for args: key: %2, value: %3",
                    toString(result), domainName, m_domainName);
            }
            handler(result == ResultCode::ok);
        });
}

void ClusterDbMapRemoteRelayPeerPool::setPublicUrl(
    const nx::utils::Url& publicUrl)
{
    m_domainName = publicUrl.port() > 0
        ? nx::network::SocketAddress(publicUrl.host(), publicUrl.port()).toStdString()
        : publicUrl.host().toStdString();

    m_syncEngineUrl = nx::network::url::Builder(publicUrl)
        .setPath(nx::network::http::rest::substituteParameters(
            m_baseApiPath,
            {m_settings.map.synchronizationSettings.clusterId}));

    startDiscovery();
}

void ClusterDbMapRemoteRelayPeerPool::registerHttpApi(
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    if (m_httpApiRegistered)
        return;

    m_messageDispatcher = messageDispatcher;

    if (m_map)
    {
        m_map->synchronizationEngine().registerHttpApi(
            m_baseApiPath,
            messageDispatcher);
        m_httpApiRegistered = true;
    }

    startDiscovery();
}

void ClusterDbMapRemoteRelayPeerPool::startDiscovery()
{
    if (m_map && !m_syncEngineUrl.isEmpty())
    {
        m_map->synchronizationEngine().discoveryManager().start(
            m_settings.map.synchronizationSettings.clusterId,
            m_syncEngineUrl);
    }
}

} // namespace nx::cloud::relay::model
