#pragma once

#include <nx/utils/url.h>

#include "traffic_relay_launcher.h"

namespace nx::cloud::relay::test {

//-------------------------------------------------------------------------------------------------
// TrafficRelay

class TrafficRelay: public Launcher
{
public:
    /**
     * NOTE: adds "-http/listenOn=127.0.0.1" arg.
     */
    TrafficRelay();

    nx::network::SocketAddress httpEndpoint(int index = 0) const;
    nx::network::SocketAddress httpsEndpoint(int index = 0) const;

    nx::utils::Url httpUrl(int index = 0) const;
    nx::utils::Url httpsUrl(int index = 0) const;
};

//-------------------------------------------------------------------------------------------------
// TrafficRelayCluster

/**
 * Represents a traffic relay cluster, adding all necessary args for discovery and synchronization.
 * NOTE: owns the traffic relays.
 * NOTE: overrides PublicIpDiscoveryService func with "127.0.0.1".
 * NOTE: neither start() nor startAndWaitUntilStarted() are called, they must be called by the
 * user.
 */
class TrafficRelayCluster
{
public:
    TrafficRelayCluster(const nx::utils::Url& discoveryServiceUrl);

    void stopAllRelays();

    int size() const;
    bool empty() const;
    void clear();

    TrafficRelay& addRelay();
    TrafficRelay& relay(int index);
    const TrafficRelay& relay(int index) const;

    bool peerInformationSynchronizedInCluster(const std::string& hostname) const;

private:
    void addClusterArgs(int index, TrafficRelay* relay);

private:
    nx::utils::Url m_discoveryServiceUrl;
    std::vector <std::unique_ptr<TrafficRelay>> m_relays;
};

} // namespace nx::cloud::relay::test
