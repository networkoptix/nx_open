#pragma once

#include <nx/network/http/test_http_server.h>
#include <nx/sql/test_support/test_with_db_helper.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/url.h>
#include <nx/utils/test_support/module_instance_launcher.h>

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

    bool peerInformationSynchronizedInCluster(
        const std::vector<std::string>& hostnames);

    std::unique_ptr<nx::network::http::TestHttpServer> addListeningPeer(
        const std::string& listeningPeerHostName,
        network::ssl::EncryptionUse encryptionUse = network::ssl::EncryptionUse::never);

    std::vector<std::unique_ptr<nx::network::http::TestHttpServer>> addListeningPeers(
        const std::vector<std::string>& listeningPeerHostName,
        network::ssl::EncryptionUse encryptionUse = network::ssl::EncryptionUse::never);

private:
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    std::unique_ptr<TrafficRelayCluster> m_relayCluster;
    ListeningPeerPool m_listeningPeerPool;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
