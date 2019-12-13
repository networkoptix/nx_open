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
    OnlineRelaysClusterClient(
        const conf::Settings& settings,
        nx::geo_ip::AbstractResolver* resolver);
    ~OnlineRelaysClusterClient();

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        const nx::network::HostAddress& serverHost,
        RelayInstanceSelectCompletionHandler completionHandler) override;

    virtual void findRelayInstanceForClient(
        const std::string& peerId,
        const nx::network::HostAddress& clientHost,
        RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    struct RelayContext
    {
        nx::geo_ip::Location location;
        nx::cloud::discovery::Node node;
        bool operator<(const RelayContext& rhs)const
        {
            return node < rhs.node;
        }
    };

    void onRelayDiscovered(nx::cloud::discovery::Node node);
    void onRelayLost(nx::cloud::discovery::Node node);

    std::vector<nx::utils::Url> findRelaysByLocation(
        const std::string& entity,
        const std::optional<nx::geo_ip::Location>& location);

    std::vector<nx::utils::Url> findRelaysByContinent(
        nx::geo_ip::Continent continent) const;

    /**
     * @return List of relay URLs sorted by distance from geopoint ascending.
     */
    std::vector<nx::utils::Url> findClosestRelays(
        const nx::geo_ip::Geopoint& geopoint) const;

    std::vector<nx::utils::Url> getUnresolvedRelays() const;

    std::optional<nx::geo_ip::Location> resolve(
        const std::string& peerType,
        const std::string& ipAddress);

private:
    const conf::Settings& m_settings;

    nx::geo_ip::AbstractResolver* m_geoIpResolver = nullptr;
    nx::cloud::discovery::DiscoveryClient m_trafficRelayDiscoveryClient;

    mutable QnMutex m_mutex;
    std::multimap<nx::geo_ip::Continent, RelayContext> m_resolvedRelays;
    std::set<nx::cloud::discovery::Node> m_unresolvedRelays;

    nx::network::aio::BasicPollable m_aioThreadBinder;
};

} // namespace nx::hpm