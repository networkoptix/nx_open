#include "listening_peer_db.h"

#include <nx/utils/std/algorithm.h>
#include <nx/clusterdb/engine/http/http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/url/url_builder.h>

namespace nx::hpm {

namespace {

static std::string toLowerReversed(std::string domainName)
{
    nx::utils::to_lower(&domainName);
    return nx::utils::reverseWords(domainName, ".");
}

int toInt(const std::string& integer)
{
    try
    {
        return std::stoi(integer);
    }
    catch (const std::exception& /*e*/)
    {
        NX_WARNING(
            typeid(ListeningPeerDb),
            "error converting string to int: %1",
            integer);
        return MediatorEndpoint::kPortUnused;
    }
}

std::string toString(const MediatorEndpoint& endpoint)
{
    std::string s = endpoint.domainName;
    s += ";";
    s += std::to_string(endpoint.httpPort);
    s += ";";
    s += std::to_string(endpoint.httpsPort);
    s += ";";
    s += std::to_string(endpoint.stunUdpPort);
    return s;
}


std::optional<MediatorEndpoint> toMediatorEndpoint(const std::string& endpointStr)
{
    std::vector<std::string> values;
    boost::split(values, endpointStr, boost::is_any_of(";"));
    if (values.size() != 4)
        return std::nullopt;

    return MediatorEndpoint{
        std::move(values[0]),
        toInt(values[1]),
        toInt(values[2]),
        toInt(values[3])
    };
}

} //namespace

//-------------------------------------------------------------------------------------------------
// MediatorEndpoint

bool MediatorEndpoint::operator==(const MediatorEndpoint &other) const
{
    return
        domainName == other.domainName
        && httpPort == other.httpPort
        && httpsPort == other.httpsPort
        && stunUdpPort == other.stunUdpPort;
}

//-------------------------------------------------------------------------------------------------
// ListeningPeerDb

ListeningPeerDb::ListeningPeerDb(const conf::ClusterDbMap& settings):
    m_settings(settings)
{
}

bool ListeningPeerDb::initialize()
{
    if (m_map)
        return true;

    if (!m_settings.enabled)
    {
        NX_INFO(this, "Starting in stand-alone mode. "
            "Not synchronizing listening peer list with other nodes");
        return true;
    }

    NX_INFO(this, "Starting in cluster mode. Initializing listening peer DB...");

    try
    {
        auto sqlExecutor = std::make_unique<nx::sql::AsyncSqlQueryExecutor>(m_settings.sql);
        // Throws exception
        auto map = std::make_unique<nx::clusterdb::map::EmbeddedDatabase>(
            m_settings.map,
            sqlExecutor.get());

        m_sqlExecutor = std::move(sqlExecutor);
        m_map = std::move(map);
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, "Error initializing EmbeddedDatabase: %1", e.what());
    }

    return m_map != nullptr;
}

void ListeningPeerDb::setThisMediatorEndpoint(const MediatorEndpoint& endpoint)
{
    m_mediatorEndpoint = endpoint;
    m_mediatorEndpointString = toString(endpoint);

    NX_ASSERT(toMediatorEndpoint(m_mediatorEndpointString) == m_mediatorEndpoint);

    m_syncEngineUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(endpoint.domainName.c_str())
        .setPath(nx::network::http::rest::substituteParameters(
            nx::clusterdb::engine::kBaseSynchronizationPath,
            {m_settings.map.synchronizationSettings.clusterId}).c_str())
        .setPort(endpoint.httpPort).toUrl();
}

const MediatorEndpoint& ListeningPeerDb::thisMediatorEndpoint() const
{
    return m_mediatorEndpoint;
}

void ListeningPeerDb::addPeer(
    const std::string& peerDomainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    if (!m_map || m_mediatorEndpointString.empty())
        return handler(false);

    m_map->database().dataManager().insertOrUpdate(
        toLowerReversed(peerDomainName),
        m_mediatorEndpointString,
        [this, peerDomainName, handler = std::move(handler)](
            nx::clusterdb::map::ResultCode result)
        {
            if (result != nx::clusterdb::map::ResultCode::ok)
            {
                NX_WARNING(
                    this,
                    "insertOrUpdate peerDomainName(key): %1, mediatorDomainName(value): %2, "
                        "failed with error: %3",
                    peerDomainName, m_mediatorEndpointString, nx::clusterdb::map::toString(result));
                return handler(false);
            }

            return handler(true);
        });
}

void ListeningPeerDb::removePeer(
    const std::string& peerDomainName,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    if (!m_map)
        return handler(false);

    m_map->database().dataManager().remove(
        toLowerReversed(peerDomainName),
        [this, peerDomainName, handler = std::move(handler)](
            nx::clusterdb::map::ResultCode result)
        {
            if (result != nx::clusterdb::map::ResultCode::ok)
            {
                NX_WARNING(
                    this,
                    "removing peerDomainName: %1 failed with error: %3",
                    peerDomainName, nx::clusterdb::map::toString(result));
                return handler(false);
            }

            return handler(true);
        });
}

void ListeningPeerDb::findMediatorByPeerDomain(
    const std::string& peerDomainName,
    nx::utils::MoveOnlyFunc<void(MediatorEndpoint)> handler)
{
   if (!m_map)
        return handler(MediatorEndpoint());

    m_map->database().dataManager().getRangeWithPrefix(
        toLowerReversed(peerDomainName),
        [this, peerDomainName, handler = std::move(handler)](
            nx::clusterdb::map::ResultCode result, std::map<std::string, std::string> map)
        {
            if (result != nx::clusterdb::map::ResultCode::ok)
            {
                NX_WARNING(
                    this,
                    "getRangeWithPrefix returned ResultCode: %1 for peerDomainName: %2",
                    nx::clusterdb::map::toString(result), peerDomainName);
                return handler(MediatorEndpoint());
            }

            NX_VERBOSE(
                this,
                "getRangeWithPrefix returned ResultCode: %1 and result set: %2 for peerDomainName: %3",
                nx::clusterdb::map::toString(result), containerString(map), peerDomainName);

            if (map.empty())
                return handler(MediatorEndpoint());

            auto endpoint = toMediatorEndpoint(map.begin()->second);
            if (!endpoint.has_value())
            {
                NX_WARNING(this,
                    "Failed to deserialize MediatorEndpoint. string was: %1",
                    map.begin()->second);
                return handler(MediatorEndpoint());
            }

            return handler(*endpoint);
        });
}

void ListeningPeerDb::startDiscovery(
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    if (!m_map)
        return;

    m_map->synchronizationEngine().registerHttpApi(
        nx::clusterdb::engine::kBaseSynchronizationPath,
        messageDispatcher);

    m_map->synchronizationEngine().discoveryManager().start(
        m_settings.map.synchronizationSettings.clusterId,
        m_syncEngineUrl);
}

std::string ListeningPeerDb::nodeId() const
{
    const auto& nodeId = m_settings.map.synchronizationSettings.nodeId;
    if (!nodeId.empty())
        return nodeId;

    return m_map
        ? m_map->synchronizationEngine().peerId().toSimpleString().toStdString()
        : std::string();
}

} // namespace nx::hpm
