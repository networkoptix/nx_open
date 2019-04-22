#include "traffic_relay_cluster.h"

#include <nx/utils/thread/sync_queue.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx::cloud::relay::test {

namespace {

static constexpr char kClusterId[] = "traffic_relay_test_cluster";

} // namespace

//-------------------------------------------------------------------------------------------------
// TrafficRelay

TrafficRelay::TrafficRelay()
{
    addArg("-http/listenOn", "127.0.0.1:0");
}

nx::network::SocketAddress TrafficRelay::httpEndpoint(int index) const
{
    return moduleInstance()->httpEndpoints()[index];
}

nx::network::SocketAddress TrafficRelay::httpsEndpoint(int index) const
{
    NX_ASSERT(index < static_cast<int>(moduleInstance()->httpsEndpoints().size()));
    return moduleInstance()->httpsEndpoints().empty()
        ? nx::network::SocketAddress()
        : moduleInstance()->httpsEndpoints()[index];
}

nx::utils::Url TrafficRelay::httpUrl(int index) const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(httpEndpoint(index));
}

nx::utils::Url TrafficRelay::httpsUrl(int index) const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(httpsEndpoint(index));
}

//-------------------------------------------------------------------------------------------------
// TrafficRelayCluster

TrafficRelayCluster::TrafficRelayCluster(const nx::utils::Url& discoveryServiceUrl):
    m_discoveryServiceUrl(discoveryServiceUrl)
{
    controller::PublicIpDiscoveryService::setDiscoverFunc(
        []() { return network::HostAddress("127.0.0.1"); });
}

void TrafficRelayCluster::stopAllRelays()
{
    for (auto& relay : m_relays)
        relay->stop();
}

int TrafficRelayCluster::size() const
{
    return static_cast<int>(m_relays.size());
}

bool TrafficRelayCluster::empty() const
{
    return m_relays.empty();
}

void TrafficRelayCluster::clear()
{
    m_relays.clear();
}

TrafficRelay& TrafficRelayCluster::addRelay()
{
    m_relays.emplace_back(std::make_unique<TrafficRelay>());
    addClusterArgs(size() - 1, m_relays.back().get());
    return *m_relays.back();
}

TrafficRelay& TrafficRelayCluster::relay(int index)
{
    return *m_relays[index];
}

const TrafficRelay& TrafficRelayCluster::relay(int index) const
{
    return *m_relays[index];
}

bool TrafficRelayCluster::peerInformationSynchronizedInCluster(const std::string& hostname) const
{
    const auto doesRelayhaveHostname =
        [](const TrafficRelay* relay, const std::string& hostname)->bool
        {
            if (relay->moduleInstance()->listeningPeerPool().isPeerOnline(hostname))
                return true;

            nx::utils::SyncQueue<bool> hasHostname;
            relay->moduleInstance()->remoteRelayPeerPool().findRelayByDomain(
                hostname,
                [&hasHostname](std::string relay) { hasHostname.push(!relay.empty()); });
            return hasHostname.pop();
        };

    for (const auto& relay : m_relays)
    {
        if (!doesRelayhaveHostname(relay.get(), hostname))
            return false;
    }

    return true;
}

void TrafficRelayCluster::addClusterArgs(int index, TrafficRelay* relay)
{
    relay->addArg("-listeningPeerDb/connectionRetryDelay", "1ms");
    relay->addArg("-listeningPeerDb/cluster/discovery/registrationErrorDelay", "1ms");
    relay->addArg("-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "100ms");
    relay->addArg("-listeningPeerDb/cluster/discovery/enabled", "true");
    relay->addArg("-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms");
    relay->addArg(
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl",
        m_discoveryServiceUrl.toStdString().c_str());
    relay->addArg("-listeningPeerDb/cluster/clusterId", kClusterId);
    relay->addArg(
        "-listeningPeerDb/cluster/nodeId",
        lm("traffic_relay_node_%1").arg(index).toStdString().c_str());
    relay->addArg("-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms");
}

} //