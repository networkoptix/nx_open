#pragma once

#include "cloud_data_provider.h"
#include "discovery/registered_peer_pool.h"
#include "listening_peer_pool.h"
#include "peer_registrator.h"
#include "mediaserver_endpoint_tester.h"
#include "relay/abstract_relay_cluster_client.h"
#include "server/hole_punching_processor.h"
#include "statistics/stats_manager.h"

namespace nx {
namespace hpm {

namespace conf { class Settings; }

class Controller
{
public:
    Controller(const conf::Settings& settings);

    MediaserverEndpointTester& mediaserverEndpointTester();

    ListeningPeerPool& listeningPeerPool();

    PeerRegistrator& listeningPeerRegistrator();
    const PeerRegistrator& listeningPeerRegistrator() const;

    HolePunchingProcessor& cloudConnectProcessor();

    nx::cloud::discovery::RegisteredPeerPool& discoveredPeerPool();
    const nx::cloud::discovery::RegisteredPeerPool& discoveredPeerPool() const;

    void stop();

private:
    stats::StatsManager m_statsManager;
    std::unique_ptr<AbstractCloudDataProvider> m_cloudDataProvider;
    MediaserverEndpointTester m_mediaserverEndpointTester;
    std::unique_ptr<AbstractRelayClusterClient> m_relayClusterClient;
    ListeningPeerPool m_listeningPeerPool;
    PeerRegistrator m_listeningPeerRegistrator;
    HolePunchingProcessor m_cloudConnectProcessor;
    nx::cloud::discovery::RegisteredPeerPool m_discoveredPeerPool;
};

} // namespace hpm
} // namespace nx
