#include "abstract_relay_cluster_client.h"
#include <nx/network/aio/basic_pollable.h>

#include <nx/cloud/discovery/discovery_api_client.h>
#include <nx/network/dns_resolver.h>

#include "nx/cloud/mediator/geo_ip/abstract_geo_ip_resolver.h"

namespace nx::hpm {

namespace conf { class Settings; }

class OnlineRelaysClusterClient:
    public AbstractRelayClusterClient
{
public:
    OnlineRelaysClusterClient(const conf::Settings& settings);
    ~OnlineRelaysClusterClient();

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        RelayInstanceSelectCompletionHandler completionHandler) override;

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& peerId,
        RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    void onRelayDiscovered(nx::cloud::discovery::Node node);
    void onRelayLost(nx::cloud::discovery::Node node);

    std::vector<nx::utils::Url> findOnlineRelaysBy(geo_ip::Continent continent);

    void resolvePeerIdToContinent(
        const std::string& peerId,
        nx::utils::MoveOnlyFunc<void(geo_ip::ResultCode, geo_ip::Continent)> handler);

private:
    std::unique_ptr<geo_ip::AbstractGeoIpResolver> m_geoIpResolver;
    nx::cloud::discovery::DiscoveryClient m_trafficRelayDiscoveryClient;

    QnMutex m_mutex;
    std::multimap<geo_ip::Continent, nx::cloud::discovery::Node> m_onlineRelayLocations;
};

} // namespace nx::hpm