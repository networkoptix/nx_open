#include "online_relays_cluster_client.h"

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>

#include "nx/cloud/mediator/settings.h"

namespace nx::hpm {

namespace {

static constexpr char kTrafficRelay[] = "traffic relay";
static constexpr char kListeningPeer[] = "listening peer";
static constexpr char kClient[] = "client";

nx::utils::Url baseUrl(nx::utils::Url url)
{
    url.setPath({});
    return url;
}

}

OnlineRelaysClusterClient::OnlineRelaysClusterClient(
    const conf::Settings& settings,
    nx::geo_ip::AbstractResolver* resolver):
    m_settings(settings),
    m_geoIpResolver(resolver),
    m_trafficRelayDiscoveryClient(
        settings.trafficRelay().discovery,
        settings.trafficRelay().clusterId,
        /*nodeInfo*/ {/* Intentionally empty, we aren't registering with discovery service */})
{
    using namespace std::placeholders;

    m_trafficRelayDiscoveryClient.setOnNodeDiscovered(
        std::bind(&OnlineRelaysClusterClient::onRelayDiscovered, this, _1));

    m_trafficRelayDiscoveryClient.setOnNodeLost(
        std::bind(&OnlineRelaysClusterClient::onRelayLost, this, _1));

    m_trafficRelayDiscoveryClient.startGetOnlineNodes();
}

OnlineRelaysClusterClient::~OnlineRelaysClusterClient()
{
    m_trafficRelayDiscoveryClient.pleaseStopSync();
    m_aioThreadBinder.pleaseStopSync();
}

void OnlineRelaysClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& peerId,
    const nx::network::HostAddress& serverHost,
    RelayInstanceSelectCompletionHandler completionHandler)
{
    m_aioThreadBinder.post(
        [this, peerId, serverHost, completionHandler = std::move(completionHandler)]()
    {
        std::string listeningPeer = std::string(kListeningPeer) + ": " + peerId;
        auto location = resolve(listeningPeer, serverHost.toStdString());
        auto relayUrls = findRelaysByLocation(listeningPeer, location);
        if (relayUrls.empty())
        {
            return completionHandler(
                nx::cloud::relay::api::ResultCode::notFound,
                std::vector<nx::utils::Url>());
        }

        NX_VERBOSE(this,
            "selectRelayInstanceForListeningPeer() reporting relay urls: %1 for %2",
            containerString(relayUrls), listeningPeer);

        completionHandler(nx::cloud::relay::api::ResultCode::ok, std::move(relayUrls));
    });
}

void OnlineRelaysClusterClient::findRelayInstanceForClient(
     const std::string& peerId,
     const nx::network::HostAddress& clientHost,
     RelayInstanceSearchCompletionHandler completionHandler)
{
    m_aioThreadBinder.post(
        [this, peerId, clientHost, completionHandler = std::move(completionHandler)]()
    {
        auto location = resolve(kClient, clientHost.toStdString());
        auto relayUrls = findRelaysByLocation(kClient, location);
        if (relayUrls.empty())
        {
            return completionHandler(
                nx::cloud::relay::api::ResultCode::notFound,
                nx::utils::Url());
        }

        // Always choosing the closest relay to the client.
        auto& url = relayUrls.front();

        NX_VERBOSE(this,
            "findRelayInstanceForClient() reporting relay url: %1 for listening peer: %2"
            "and client ip: %3",
            url, peerId, clientHost);

        completionHandler(
            nx::cloud::relay::api::ResultCode::ok,
            std::move(url));
    });
}

void OnlineRelaysClusterClient::onRelayDiscovered(nx::cloud::discovery::Node trafficRelayNode)
{
    if (trafficRelayNode.urls.empty())
    {
        NX_ERROR(this, "Discovered traffic relay: %1 that provides no urls. Ignoring",
            trafficRelayNode);
        return;
    }

    auto location = resolve(kTrafficRelay, trafficRelayNode.publicIpAddress);
    if (location)
    {
        NX_VERBOSE(this, "Discovered resolved traffic relay: %1, location: %2",
            trafficRelayNode, location);
        QnMutexLocker lock(&m_mutex);
        m_resolvedRelays.emplace(
            location->continent,
            RelayContext{*location, std::move(trafficRelayNode)});
    }
    else
    {
        NX_WARNING(this, "Discoverd traffic relay: %1 but location resolution failed",
            trafficRelayNode);
        QnMutexLocker lock(&m_mutex);
        m_unresolvedRelays.emplace(std::move(trafficRelayNode));
    }
}

void OnlineRelaysClusterClient::onRelayLost(nx::cloud::discovery::Node trafficRelayNode)
{
    NX_VERBOSE(this, "Traffic relay: %1 was dropped by discovery service.", trafficRelayNode);

    QnMutexLocker lock(&m_mutex);
    for (auto it = m_resolvedRelays.begin(); it != m_resolvedRelays.end(); ++it)
    {
        if (it->second.node == trafficRelayNode)
        {
            m_resolvedRelays.erase(it);
            return;
        }
    }

    m_unresolvedRelays.erase(trafficRelayNode);
}

std::vector<nx::utils::Url> OnlineRelaysClusterClient::findRelaysByContinent(
    nx::geo_ip::Continent continent) const
{
    QnMutexLocker lock(&m_mutex);
    std::vector<nx::utils::Url> relayUrls;

    auto range = m_resolvedRelays.equal_range(continent);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second.node.urls.empty())
            continue;

        relayUrls.emplace_back(baseUrl(it->second.node.urls.front()));
    }

    return relayUrls;
}

std::vector<nx::utils::Url> OnlineRelaysClusterClient::findClosestRelays(
    const nx::geo_ip::Geopoint& geopoint) const
{
    std::vector<nx::utils::Url> urls;
    std::multimap<nx::geo_ip::Kilometers, const nx::cloud::discovery::Node&> relaysByDistance;

    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& context : m_resolvedRelays)
        {
            relaysByDistance.emplace(
                geopoint.distanceTo(context.second.location.geopoint),
                context.second.node);
        }
    }

    for (const auto& relay : relaysByDistance)
    {
        if (!relay.second.urls.empty())
            urls.emplace_back(baseUrl(relay.second.urls.front()));

        if ((int) urls.size() >= m_settings.geoIp().resolveErrorUrlCount)
            break;
    }

    return urls;
}

std::vector<nx::utils::Url> OnlineRelaysClusterClient::getUnresolvedRelays() const
{
    QnMutexLocker lock(&m_mutex);
    std::vector<nx::utils::Url> urls;
    for (const auto& relay: m_unresolvedRelays)
    {
        if (!relay.urls.empty())
            urls.emplace_back(baseUrl(relay.urls.front()));

        if ((int) urls.size() >= m_settings.geoIp().resolveErrorUrlCount)
            break;
    }

    return urls;
}

std::optional<nx::geo_ip::Location> OnlineRelaysClusterClient::resolve(
    const std::string& peerType,
    const std::string& ipAddress)
{
    try
    {
        auto location = m_geoIpResolver->resolve(ipAddress);
        NX_VERBOSE(this, "Resolved %1 with ipAddress: %2 to location: %3",
            peerType, ipAddress, location);
        return location;
    }
    catch (const std::exception& e)
    {
        NX_INFO(this, "Error resolving ip address: %1 of %2 to location: %3",
            ipAddress, peerType, e.what());
        return std::nullopt;
    }
}

std::vector<nx::utils::Url> OnlineRelaysClusterClient::findRelaysByLocation(
    const std::string& entity,
    const std::optional<nx::geo_ip::Location>& location)
{
    if (!location)
    {
        NX_INFO(this, "Failed to resolve location for %1. Falling back to urls in"
            " traffic relay settings",
            entity);
        if (m_settings.trafficRelay().urls.empty())
            NX_ERROR(this, "No default traffic relayUrls provided in settings");
        return m_settings.trafficRelay().urls;
    }

    std::vector<nx::utils::Url> urls;

    NX_DEBUG(this, "Found 0 relays in %1. Falling back to relays by distance",
        location);
    urls = findClosestRelays(location->geopoint);
    if (!urls.empty())
    {
        NX_VERBOSE(this, "Found relays by distance: %1", containerString(urls));
        return urls;
    }

    NX_ERROR(this, "Found 0 relays by distance. GeoIp resolution is not working."
        " Falling back to any unresolved relays");
    urls = getUnresolvedRelays();
    if (!urls.empty())
    {
        NX_VERBOSE(this, "Found unresolved relays: %1", containerString(urls));
        return urls;
    }

    NX_ERROR(this, "Found 0 relays, resolved or unresolved. Discovery service"
        " is not working. Falling back to relay urls from settings");

    if (m_settings.trafficRelay().urls.empty())
        NX_ERROR(this, "TrafficRelay settings do not have any fallback urls. No relays found");

    return m_settings.trafficRelay().urls;
}

} // namespace nx::hpm
