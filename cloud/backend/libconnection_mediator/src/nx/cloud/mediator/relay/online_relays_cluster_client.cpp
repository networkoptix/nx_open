#include "online_relays_cluster_client.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>

#include "nx/cloud/mediator/settings.h"
#include "nx/cloud/mediator/geo_ip/resolver_factory.h"

namespace nx::hpm {

OnlineRelaysClusterClient::OnlineRelaysClusterClient(const conf::Settings& settings):
    m_geoIpResolver(geo_ip::ResolverFactory::instance().create(settings)),
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
    const nx::network::SocketAddress& serverEndpoint,
    RelayInstanceSelectCompletionHandler completionHandler)
{
    using namespace nx::cloud::relay::api;

    m_aioThreadBinder.post(
        [this, peerId, serverEndpoint, completionHandler = std::move(completionHandler)]()
    {
        auto[resultCode, location] = m_geoIpResolver->resolve(serverEndpoint);
        if (resultCode != nx::geo_ip::ResultCode::ok)
        {
            NX_WARNING(this, "Error resolving peerId: %1 with server address: %1 to a continent",
                peerId, serverEndpoint);

            return completionHandler(ResultCode::notFound, std::vector<nx::utils::Url>());
        }

        auto relayUrls = findOnlineRelaysBy(location.continent);
        if (relayUrls.empty())
        {
            NX_WARNING(this, "There were no online relays found for continent: %1",
                nx::geo_ip::toString(location.continent));

            return completionHandler(ResultCode::notFound, std::vector<nx::utils::Url>());
        }

        NX_VERBOSE(this,
            "Resolved peerId: %1 with server endpoint: %2 to continent: %3 and found relay urls: %4",
                peerId, serverEndpoint, nx::geo_ip::toString(location.continent),
                containerString(relayUrls));

        return completionHandler(ResultCode::ok, std::move(relayUrls));
    });
}

void OnlineRelaysClusterClient::findRelayInstancePeerIsListeningOn(
     const std::string& peerId,
     const nx::network::SocketAddress& clientEndpoint,
     RelayInstanceSearchCompletionHandler completionHandler)
{
    using namespace nx::cloud::relay::api;

    m_aioThreadBinder.post(
        [this, peerId, clientEndpoint, completionHandler = std::move(completionHandler)]()
    {
        auto[resultCode, location] =
            m_geoIpResolver->resolve(clientEndpoint);

        if (resultCode != nx::geo_ip::ResultCode::ok)
        {
            NX_WARNING(this, "Error resolving peerId: %1with client address: %2 to a continent",
                peerId, clientEndpoint);
            return completionHandler(ResultCode::notFound, nx::utils::Url());
        }

        auto relayUrls = findOnlineRelaysBy(location.continent);
        if (relayUrls.empty())
        {
            NX_WARNING(this, "There were no online relays found for continent: %1",
                nx::geo_ip::toString(location.continent));
            return completionHandler(ResultCode::notFound, nx::utils::Url());
        }

        std::random_device rd;  //Will be used to obtain a seed for the random number engine
        std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> dis(0, (int)relayUrls.size() - 1);

        return completionHandler(cloud::relay::api::ResultCode::ok, relayUrls[dis(gen)]);
    });
}

void OnlineRelaysClusterClient::onRelayDiscovered(nx::cloud::discovery::Node trafficRelayNode)
{
    auto [resultCode, location] = m_geoIpResolver->resolve(trafficRelayNode.publicIpAddress);
    if (resultCode != nx::geo_ip::ResultCode::ok)
    {
        NX_WARNING(this, "Failed to resolve traffic relay: %1 to a known continent using"
            " public ip address: %2",
            trafficRelayNode, trafficRelayNode.publicIpAddress);
        return;
    }

    NX_VERBOSE(this, "Traffic relay: %1 discovered and resolved to continent: %2",
        trafficRelayNode, nx::geo_ip::toString(location.continent));

    QnMutexLocker lock(&m_mutex);
    m_onlineRelayLocations.emplace(location.continent, trafficRelayNode);
}

void OnlineRelaysClusterClient::onRelayLost(nx::cloud::discovery::Node trafficRelayNode)
{
    NX_VERBOSE(this, "Traffic relay: %1 was dropped by discovery service.", trafficRelayNode);

    QnMutexLocker lock(&m_mutex);
    for (auto it = m_onlineRelayLocations.begin(); it != m_onlineRelayLocations.end(); ++it)
    {
        if (it->second == trafficRelayNode)
        {
            m_onlineRelayLocations.erase(it);
            break;
        }
    }
}

std::vector<nx::utils::Url> OnlineRelaysClusterClient::findOnlineRelaysBy(
    nx::geo_ip::Continent continent)
{
    QnMutexLocker lock(&m_mutex);
    std::vector<nx::utils::Url> relayUrls;

    auto range = m_onlineRelayLocations.equal_range(continent);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second.urls.empty())
        {
            NX_WARNING(this, "Traffic relay: %1 is online but provides no urls. Ignoring",
                it->second);
            continue;
        }

        relayUrls.emplace_back(it->second.urls.front());
        relayUrls.back().setPath({});
    }

    return relayUrls;
}

} // namespace nx::hpm