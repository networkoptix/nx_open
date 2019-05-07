#include "abstract_relay_cluster_client.h"

#include <nx/cloud/discovery/discovery_api_client.h>
#include <nx/geo_ip/abstract_resolver.h>
#include <nx/network/aio/basic_pollable.h>

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
        const nx::network::SocketAddress& serverEndpoint,
        RelayInstanceSelectCompletionHandler completionHandler) override;

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& peerId,
        const nx::network::SocketAddress& clientEndpoint,
        RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    void onRelayDiscovered(nx::cloud::discovery::Node node);
    void onRelayLost(nx::cloud::discovery::Node node);

    std::vector<nx::utils::Url> findOnlineRelaysBy(nx::geo_ip::Continent continent);

private:
    std::unique_ptr<nx::geo_ip::AbstractResolver> m_geoIpResolver;
    nx::cloud::discovery::DiscoveryClient m_trafficRelayDiscoveryClient;

    QnMutex m_mutex;
    std::multimap<nx::geo_ip::Continent, nx::cloud::discovery::Node> m_onlineRelayLocations;

    nx::network::aio::BasicPollable m_aioThreadBinder;
};

} // namespace nx::hpm