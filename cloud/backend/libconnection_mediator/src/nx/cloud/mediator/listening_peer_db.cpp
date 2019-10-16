#include "listening_peer_db.h"

#include <nx/utils/std/algorithm.h>
#include <nx/clusterdb/engine/http/http_paths.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/stun/stun_types.h>
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

struct Urls
{
    std::string tcp;
    std::string udp;
};

struct MediatorInfoJson
{
    Urls mediatorUrls;
};

#define MediatorInfoJson_Fields (mediatorUrls)
#define Urls_Fields (tcp)(udp)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (MediatorInfoJson)(Urls),
    (json),
    _Fields)

} //namespace

//-------------------------------------------------------------------------------------------------
// ListeningPeerDb

ListeningPeerDb::ListeningPeerDb(const conf::Settings& settings):
    m_settings(settings.listeningPeerDb()),
    m_mediatorSelector(MediatorSelectorFactory::instance().create(settings))
{
    // Disabling timeout so that queries are not cancelled by timeout.
    // Currently, the implementation ignores DB save error.
    // So, we need a way to minimize such errors.
    m_settings.sql.maxPeriodQueryWaitsForAvailableConnection =
        std::chrono::milliseconds::zero();

    // Speeding up saving to the DB in case of high load (e.g., just after start).
    m_settings.map.synchronizationSettings.groupCommandsUnderDbTransaction = true;
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

void ListeningPeerDb::stop()
{
    if (m_map)
        m_map->synchronizationEngine().pleaseStopSync();
    if (m_sqlExecutor)
        m_sqlExecutor->pleaseStopSync();
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
        toInternalStorageFormat(peerDomainName),
        m_mediatorEndpointString,
        [this, peerDomainName, handler = std::move(handler)](
            auto result)
        {
            if (!result.ok())
            {
                NX_WARNING(this,
                    "insertOrUpdate peerDomainName(key): %1, mediatorDomainName(value): %2, "
                        "failed with error: {%3}",
                    peerDomainName, m_mediatorEndpointString, result);
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
        toInternalStorageFormat(peerDomainName),
        [this, peerDomainName, handler = std::move(handler)](auto result)
        {
            if (!result.ok())
            {
                NX_WARNING(this,
                    "removing peerDomainName: %1 failed with error: {%3}",
                    peerDomainName, result);
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
        [this, peerDomainName, handler = std::move(handler)](auto result, auto map)
        {
            if (!result.ok())
            {
                NX_VERBOSE(this,
                    "getRangeWithPrefix returned Result: {%1} for peerDomainName: %2",
                    result, peerDomainName);
                return handler(MediatorEndpoint());
            }

            NX_VERBOSE(this,
                "getRangeWithPrefix returned ResultCode: {%1} and result set: %2 for peerDomainName: %3",
                result, containerString(map), peerDomainName);

            if (map.empty())
                return handler(MediatorEndpoint());

            bool ok = true;
            std::vector<MediatorEndpoint> endpoints;
            std::transform(map.begin(), map.end(), std::back_inserter(endpoints),
                [&ok](const auto& element)
                {
                    auto endpoint = toMediatorEndpoint(element.second);
                    ok &= endpoint.has_value();
                    return endpoint ? *endpoint : MediatorEndpoint();
                });

            if (!ok)
            {
                NX_VERBOSE(this,
                    "Failed to deserialize MediatorEndpoints. string was: %1",
                    containerString(map));
                return handler(MediatorEndpoint());
            }

            return handler(m_mediatorSelector->select(endpoints));
        });
}

void ListeningPeerDb::startDiscovery(
    const MediatorEndpoint& endpoint,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    if (!m_map)
        return;

    setThisMediatorEndpoint(endpoint);

    m_map->synchronizationEngine().discoveryManager().updateInformation(buildInfoJson(endpoint));

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
            { m_settings.map.synchronizationSettings.clusterId }).c_str())
        .setPort(endpoint.httpPort).toUrl();
}

std::string ListeningPeerDb::toInternalStorageFormat(const std::string& peerDomainName) const
{
    std::string s;
    s.reserve(peerDomainName.size() + m_mediatorEndpointString.size() + 1);
    s += toLowerReversed(peerDomainName);
    return s.append(".").append(m_mediatorEndpointString);
}

std::string ListeningPeerDb::buildInfoJson(const MediatorEndpoint& endpoint) const
{
    MediatorInfoJson infoJson;
    infoJson.mediatorUrls.tcp = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(endpoint.domainName.c_str())
        .setPort(endpoint.httpPort)
        .setPath(nx::hpm::api::kMediatorApiPrefix).toString().toStdString();

    infoJson.mediatorUrls.udp = nx::network::url::Builder()
        .setScheme(nx::network::stun::kUrlSchemeName)
        .setHost(endpoint.domainName.c_str())
        .setPort(endpoint.stunUdpPort).toString().toStdString();

    return QJson::serialized(infoJson).constData();
}

} // namespace nx::hpm
