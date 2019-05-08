#pragma once

#include <nx/utils/std/optional.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include <nx/sql/test_support/test_with_db_helper.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

#include <nx/cloud/relay/test_support/traffic_relay_cluster.h>

#include "remote_relay_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

using Relay = TrafficRelay;

//-------------------------------------------------------------------------------------------------

class BasicComponentTest:
    public nx::sql::test::TestWithDbHelper
{
    using base_type = nx::sql::test::TestWithDbHelper;

public:
    enum class Mode
    {
        singleRelay,
        cluster,
    };

    BasicComponentTest(Mode mode = Mode::cluster);

    void addRelayInstance(
        std::vector<const char*> args = {},
        bool waitUntilStarted = true);

    Relay& relay(int index = 0);
    const Relay& relay(int index = 0) const;

    void stopAllInstances();

    /**
     * @return true, if information about peer is available to every relay peer.
     */
    bool peerInformationSynchronizedInCluster(const std::string& hostname);

private:
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    bool m_serverListening = false;
    std::unique_ptr<TrafficRelayCluster> m_relayCluster;
    ListeningPeerPool m_listeningPeerPool;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
