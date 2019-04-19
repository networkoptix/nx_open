#include "remote_relay_peer_pool.h"

#include <sstream>

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>

#include <nx/network/url/url_builder.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/clusterdb/engine/http/http_paths.h>

#include "nx/cloud/relay/relay_selection/relay_selector_factory.h"

namespace nx::cloud::relay::model {

namespace {

static std::string toLowerReversed(std::string domainName)
{
    nx::utils::to_lower(&domainName);
    return nx::utils::reverseWords(domainName, ".");
}

} //namespace

RemoteRelayPeerPool::RemoteRelayPeerPool(const conf::Settings& settings):
    m_settings(settings.listeningPeerDb()),
    m_relaySelector(RelaySelectorFactory::instance().create(settings)),
    m_baseApiPath(nx::clusterdb::engine::kBaseSynchronizationPath)
{
    // Try to connect immediately
    connectToDb();
}

RemoteRelayPeerPool::~RemoteRelayPeerPool()
{
    if (m_map)
        m_map->synchronizationEngine().pleaseStopSync();
}

bool RemoteRelayPeerPool::connectToDb()
{
    if (m_map)
        return true;

    if (!m_settings.enabled)
    {
        NX_INFO(this,
            "RemoteRelayPeerPool was not enabled. Other DB's will not be discovered...");
        return true;
    }

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
        NX_ERROR(this,
            "Failed to initialize database, traffic relay will not function properly: %1",
            e.what());
    }

    return isConnected();
}

bool RemoteRelayPeerPool::isConnected() const
{
    if (!m_settings.enabled)
        return true;
    return m_map != nullptr;
}

void RemoteRelayPeerPool::findRelayByDomain(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(std::string)> handler) const
{
    using namespace nx::clusterdb::map;

    if (!m_map)
        return handler(std::string());

    // NOTE: toLowerReversed() is used here in stead of toInternalStorageFormat() because we want
    // to search only for the server's domain name.
    m_map->database().dataManager().getRangeWithPrefix(
        toLowerReversed(domainName),
        [this, domainName, handler = std::move(handler)](
            ResultCode result, std::map<std::string, std::string> map)
        {
            if (result != ResultCode::ok)
            {
                NX_WARNING(this, "getRangeWithPrefix returned error: %1 for key: %2",
                    toString(result), domainName);
                return handler(std::string());
            }

            NX_VERBOSE(this, "getRangeWithPrefix returned result set: %2 for domainName: %3",
                containerString(map), domainName);

            if (map.empty())
                return handler(std::string());


            std::vector<std::string> relayDomains;
            for (auto& peerAndRelayDomains : map)
            {
                // Avoid redirecting to this relay instance by not adding this relay to the list.
                if (peerAndRelayDomains.second.find(m_domainName) == std::string::npos)
                    relayDomains.emplace_back(std::move(peerAndRelayDomains.second));
            }

            return handler(m_relaySelector->selectRelay(relayDomains));
        });
}

void RemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    using namespace nx::clusterdb::map;

    if (!m_map)
        return handler(false);

    m_map->database().dataManager().insertOrUpdate(
        toInternalStorageFormat(domainName),
        m_domainName,
        [this, domainName, handler = std::move(handler)](ResultCode result)
        {
            if (result != ResultCode::ok)
            {
                NX_WARNING(this, "insertOrUpdate returned error: %1 for args: key: %2, value: %3",
                    toString(result), domainName, m_domainName);
            }
            handler(result == ResultCode::ok);
        });
}

void RemoteRelayPeerPool::removePeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    using namespace nx::clusterdb::map;

    if (!m_map)
        return handler(false);

    m_map->database().dataManager().remove(
        toInternalStorageFormat(domainName),
        [this, domainName, handler = std::move(handler)](ResultCode result)
        {
            if (result != ResultCode::ok)
            {
                NX_VERBOSE(this,
                    "remove returned error: %1, for args: key: %2, value: %3",
                    toString(result), domainName, m_domainName);
            }
            handler(result == ResultCode::ok);
        });
}

void RemoteRelayPeerPool::setPublicUrl(
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

void RemoteRelayPeerPool::registerHttpApi(
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

void RemoteRelayPeerPool::startDiscovery()
{
    if (m_map && !m_syncEngineUrl.isEmpty())
    {
        m_map->synchronizationEngine().discoveryManager().start(
            m_settings.map.synchronizationSettings.clusterId,
            m_syncEngineUrl);
    }
}

std::string RemoteRelayPeerPool::toInternalStorageFormat(const std::string& peerDomain) const
{
    std::string s;
    s.reserve(peerDomain.size() + m_domainName.size() + 1);
    s += toLowerReversed(peerDomain);
    return s.append(".").append(m_domainName);
}

//-------------------------------------------------------------------------------------------------
//RemoteRelayPeerPoolFactory

RemoteRelayPeerPoolFactory::RemoteRelayPeerPoolFactory():
    base_type(std::bind(&RemoteRelayPeerPoolFactory::defaultFactory, this,
        std::placeholders::_1))
{
}

RemoteRelayPeerPoolFactory& RemoteRelayPeerPoolFactory::instance()
{
    static RemoteRelayPeerPoolFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<model::AbstractRemoteRelayPeerPool>
    RemoteRelayPeerPoolFactory::defaultFactory(
        const conf::Settings& settings)
{
    return std::make_unique<RemoteRelayPeerPool>(settings);
}

} // namespace nx::cloud::relay::model
