#include "listening_peer_db.h"

#include <typeinfo>

#include <nx/utils/std/algorithm.h>
#include <nx/clusterdb/engine/http/http_paths.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>

#include "settings.h"

namespace nx::hpm {

namespace {

// The following constants are used as key type identifiers. one of them is suffixed to the end
// of each key placed in the db so that the values can be deserialized to the right type.
static constexpr char kUplinkSpeedId[] = "#uplinkSpeed";
static constexpr char kMediatorEndpointId[] = "#mediatorEndpoint";

static std::pair<std::string, std::string> splitPeerId(const std::string& peerId)
{
    std::vector<std::string> result;
    boost::split(result, peerId, boost::is_any_of("."));
    if (result.size() != 2)
    {
        NX_WARNING(NX_SCOPE_TAG, "peerId: %1 is missing required delimitter '.'", peerId);
        return {};
    }
    return {std::move(result[0]),std::move(result[1])};
}

/**
 * @return an std::pair containing {<serverId>, <systemId>}
 */
static std::pair<std::string, std::string> parsePeerId(const std::string& key)
{
    // Key is expected to be of the form: <systemId>.<serverId>#<keyTypeId>
    // The return value will have the form: <serverId>.<systemId>

    // head == systemId
    // tail == serverId#<key-type-id>
    auto [head, tail] = splitPeerId(key);

    std::vector<std::string> result;
    boost::split(result, tail, boost::is_any_of("#"));
    if (result.empty())
    {
        NX_WARNING(NX_SCOPE_TAG, "key: %1 is missing expected delimitter: '#'", key);
        return {};
    }

    // reversing systemId.serverId to serverId.systemId.
    return {result[0], head};
}

template<typename Output>
static std::optional<Output> deserialize(const std::string& string, const char* callingFunc)
{
    Output output;
    if (!QJson::deserialize(QByteArray(string.c_str()), &output))
    {
        NX_WARNING(NX_SCOPE_TAG, "%1: failed to deserialize string: %2 to type: %3",
            callingFunc, string, typeid(Output).name());
        return std::nullopt;
    }
    return output;
}

static std::string toLowerReversed(std::string domainName)
{
    nx::utils::to_lower(&domainName);
    return nx::utils::reverseWords(domainName, '.');
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

} // namespace

//-------------------------------------------------------------------------------------------------
// ListeningPeerDb

ListeningPeerDb::ListeningPeerDb(const conf::Settings& settings):
    m_settings(settings.listeningPeerDb()),
    m_mediatorSelector(MediatorSelectorFactory::instance().create(settings))
{
}

ListeningPeerDb::~ListeningPeerDb()
{
    stop();
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
        m_stopped = false;
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, "Error initializing EmbeddedDatabase: %1", e.what());
    }

    return m_map != nullptr;
}

void ListeningPeerDb::stop()
{
    if (m_stopped)
        return;

    NX_VERBOSE(this, "Stopping...");

    m_stopped = true;
    if (m_map)
        m_map->synchronizationEngine().pleaseStopSync();
    if (m_sqlExecutor)
        m_sqlExecutor->pleaseStopSync();

    NX_VERBOSE(this, "Stop complete");
}

void ListeningPeerDb::setThisMediatorEndpoint(const MediatorEndpoint& endpoint)
{
    m_mediatorEndpoint = endpoint;
    m_mediatorEndpointString = QJson::serialized(endpoint).toStdString();

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
    if (!m_map || m_mediatorEndpointString.empty() || m_stopped)
        return handler(false);

    m_map->database().dataManager().insertOrUpdate(
        toInternalStorageFormat(peerDomainName),
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
    if (!m_map || m_stopped)
        return handler(false);

    m_map->database().dataManager().remove(
        toInternalStorageFormat(peerDomainName),
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
   if (!m_map || m_stopped)
        return handler(MediatorEndpoint());

    m_map->database().dataManager().getRangeWithPrefix(
        toLowerReversed(peerDomainName),
        [this, peerDomainName, handler = std::move(handler)](
            nx::clusterdb::map::ResultCode result, std::map<std::string, std::string> map)
        {
            if (result != nx::clusterdb::map::ResultCode::ok)
            {
                NX_VERBOSE(this,
                    "getRangeWithPrefix returned ResultCode: %1 for peerDomainName: %2",
                    nx::clusterdb::map::toString(result), peerDomainName);
                return handler(MediatorEndpoint());
            }

            NX_VERBOSE(this,
                "getRangeWithPrefix returned ResultCode: %1 and result set: %2 for peerDomainName: %3",
                nx::clusterdb::map::toString(result), containerString(map), peerDomainName);

            if (map.empty())
                return handler(MediatorEndpoint());

            bool ok = true;
            std::vector<MediatorEndpoint> endpoints;
            std::transform(map.begin(), map.end(), std::back_inserter(endpoints),
                [&ok, func = __func__](const auto& element)
                {
                    auto endpoint =
                        deserialize<MediatorEndpoint>(element.second, func);
                    ok &= endpoint.has_value();
                    return endpoint ? *endpoint : MediatorEndpoint();
                });

            if (!ok)
            {
                NX_VERBOSE(this, "Failed to deserialize MediatorEndpoints. string was: %1",
                    containerString(map));
                return handler(MediatorEndpoint());
            }

            return handler(m_mediatorSelector->select(endpoints));
        });
}

void ListeningPeerDb::addUplinkSpeed(
    const std::string& peerId,
    const nx::hpm::api::ConnectionSpeed& uplinkSpeed,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    if (!m_map || m_stopped)
        return handler(false);

    auto key = toLowerReversed(peerId) + kUplinkSpeedId;

    m_map->database().dataManager().insertOrUpdate(
        key,
        QJson::serialized(uplinkSpeed).toStdString(),
        [this, key, peerId, uplinkSpeed,
            handler = std::move(handler)](
                clusterdb::map::ResultCode result)
        {
            if (result != clusterdb::map::ResultCode::ok)
            {
                NX_VERBOSE(this, "Failed to save connection speed for peer %1. %2",
                    key, clusterdb::map::toString(result));
                return handler(false);
            }

            auto [serverId, systemId] = splitPeerId(peerId);
            m_uplinkSpeedUpdated.notify(
                nx::hpm::api::PeerConnectionSpeed{
                    std::move(serverId),
                    std::move(systemId),
                    uplinkSpeed});

            handler(true);
        });
}

void ListeningPeerDb::startDiscovery(
    const MediatorEndpoint& endpoint,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    if (!m_map || m_stopped)
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

std::map<std::string, ListeningPeerStatus> ListeningPeerDb::getListeningPeerStatus(
    const std::string& peerId) const
{
    if (!m_map)
        return {};

    auto range = m_map->cache()->getRangeWithPrefix(toLowerReversed(peerId));
    if (range.empty())
    {
        NX_VERBOSE(this, "No values found in cache for peerId: %1", peerId);
        return {};
    }

    std::map<std::string, ListeningPeerStatus> result;

    for (auto element : range)
    {
        auto [serverId, systemId] = parsePeerId(element.first);
        if (serverId.empty() || systemId.empty())
            continue;

        auto peerIdParsed = serverId + "." + systemId;
        result[peerIdParsed].serverId = std::move(serverId);
        result[peerIdParsed].systemId = std::move(systemId);

        if (element.first.find(kMediatorEndpointId) != std::string::npos)
        {
            auto endpoint = deserialize<MediatorEndpoint>(element.second, __func__);
            if (!endpoint)
                continue;
            result[peerIdParsed].connectedEndpoints.push_back(std::move(*endpoint));
        }
        else if (element.first.find(kUplinkSpeedId) != std::string::npos)
        {
            auto uplinkSpeed =
                deserialize<nx::hpm::api::ConnectionSpeed>(element.second, __func__);
            if (!uplinkSpeed)
                continue;
            result[peerIdParsed].uplinkSpeed = std::move(*uplinkSpeed);
        }
        else
        {
            NX_WARNING(this, "Unknown key value pair found in cache: {%1, %2}",
                element.first, element.second);
        }
    }

    for (auto it = result.begin(); it != result.end();)
    {
        // An empty list of endpoints means that the peer has disconnected from all mediators
        // and should not be included.
        if (it->second.connectedEndpoints.empty())
            it = result.erase(it);
        else
            ++it;
    }

    return result;
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

void nx::hpm::ListeningPeerDb::subscribeToUplinkSpeedUpdated(
    nx::utils::MoveOnlyFunc<void(nx::hpm::api::PeerConnectionSpeed)> handler,
	nx::utils::SubscriptionId* const outId)
{
    m_uplinkSpeedUpdated.subscribe(std::move(handler), outId);
}

void nx::hpm::ListeningPeerDb::unsubscribeFromUplinkSpeedUpdated(nx::utils::SubscriptionId id)
{
    m_uplinkSpeedUpdated.removeSubscription(id);
}

std::string ListeningPeerDb::toInternalStorageFormat(const std::string& peerDomainName) const
{
    // NOTE: m_mediatorEndpointString is added to the key to guarantee uniqueness.
    std::string s;
    s.reserve(
        peerDomainName.size() + m_mediatorEndpointString.size() + 1 + sizeof(kMediatorEndpointId));
    s += toLowerReversed(peerDomainName);
    return s.append("#").append(m_mediatorEndpointString).append(kMediatorEndpointId);
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
