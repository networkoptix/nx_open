#include "online_relays_cluster_client.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>

#include "nx/cloud/mediator/settings.h"
#include "nx/cloud/mediator/geo_ip/geo_ip_resolver_factory.h"

namespace nx::hpm {

using namespace nx::cloud;

OnlineRelaysClusterClient::OnlineRelaysClusterClient(const conf::Settings& settings):
    m_geoIpResolver(geo_ip::GeoIpResolverFactory::instance().create(settings)),
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
    nx::network::SocketGlobals::addressResolver().cancel(this);
}

void OnlineRelaysClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& peerId,
    RelayInstanceSelectCompletionHandler completionHandler)
{
    resolvePeerIdToContinent(
        peerId,
        [this, peerId, completionHandler = std::move(completionHandler)](
            geo_ip::ResultCode resultCode, geo_ip::Continent continent)
        {
            if (resultCode != geo_ip::ResultCode::ok)
            {
                return completionHandler(
                    relay::api::ResultCode::notFound,
                    std::vector<nx::utils::Url>());
            }

            auto relayUrls = findOnlineRelaysBy(continent);

            return completionHandler(
                !relayUrls.empty()
                    ? relay::api::ResultCode::ok
                    : relay::api::ResultCode::notFound,
                std::move(relayUrls));
        });
}

void OnlineRelaysClusterClient::findRelayInstancePeerIsListeningOn(
     const std::string& peerId,
     RelayInstanceSearchCompletionHandler completionHandler)
{
    selectRelayInstanceForListeningPeer(
        peerId,
        [this, peerId, completionHandler = std::move(completionHandler)](
            relay::api::ResultCode resultCode,
            std::vector<nx::utils::Url> relayUrls)
        {
            if (resultCode != relay::api::ResultCode::ok || relayUrls.empty())
                return completionHandler(resultCode, nx::utils::Url());

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution dis(0, (int)relayUrls.size() - 1);

            return completionHandler(resultCode, relayUrls[dis(gen)]);
        }
    );
}

void OnlineRelaysClusterClient::onRelayDiscovered(nx::cloud::discovery::Node trafficRelayNode)
{
    auto [resultCode, continent] = m_geoIpResolver->resolve(trafficRelayNode.publicIpAddress);
    if (resultCode != geo_ip::ResultCode::ok)
    {
        NX_WARNING(this, "Failed to resolve traffic relay: %1 to a known continent.",
            trafficRelayNode);
        return;
    }
    NX_VERBOSE(this, "Traffic relay: %1 discovered and resolved to continent: %2",
        trafficRelayNode, geo_ip::toString(continent));

    QnMutexLocker lock(&m_mutex);
    m_onlineRelayLocations.emplace(continent, trafficRelayNode);
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
    geo_ip::Continent continent)
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

void OnlineRelaysClusterClient::resolvePeerIdToContinent(
    const std::string& peerId,
    nx::utils::MoveOnlyFunc<void(geo_ip::ResultCode, geo_ip::Continent)> handler)
{
    nx::network::SocketGlobals::addressResolver().resolveAsync(
        peerId,
        [this, peerId, handler = std::move(handler)](
            SystemError::ErrorCode errorCode,
            std::deque<network::AddressEntry> entries) mutable
        {
            if (errorCode != SystemError::noError)
            {
                NX_VERBOSE(this, "System error: %1 occured while resolving ip address of peer: %2",
                    SystemError::toString(errorCode), peerId);
                return handler(geo_ip::ResultCode::unknownError, geo_ip::Continent{});
            }

            if (entries.empty())
            {
                NX_VERBOSE(this, "No Ip address resolved for peer: %1", peerId);
                return handler(geo_ip::ResultCode::unknownError, geo_ip::Continent{});
            }

            std::string ipAddress = entries.front().toEndpoint().address.toStdString();

            NX_VERBOSE(this, "Resolved peerId: %1 to ip address: %2", peerId, ipAddress);

            auto [resultCode, continent] = m_geoIpResolver->resolve(ipAddress);
            if (resultCode != geo_ip::ResultCode::ok)
            {
                NX_VERBOSE(this, "Failed to resolve peerId: %1 with ip address:%2 to a continent",
                    peerId, ipAddress);
            }
            else
            {
                NX_VERBOSE(this, "peerId: %1 resolved to continent: %2",
                    peerId, geo_ip::toString(continent));
            }

            return handler(resultCode, continent);
        },
        network::NatTraversalSupport::disabled,
        AF_INET,
        this);
}


} // namespace nx::hpm