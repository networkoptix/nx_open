#include <gtest/gtest.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

//-------------------------------------------------------------------------------------------------
// DiscoveryTestFixture

class DiscoveryTestFixture
{
public:
    class NodeContext
    {
    public:
        NodeContext(Peer& peer):
            m_peer(peer)
        {
        }

        nx::utils::Url syncEngineUrl() const
        {
            return m_peer.baseApiUrl();
        }

        SynchronizationEngine& syncEngine()
        {
            return m_peer.process().moduleInstance()->synchronizationEngine();
        }

        engine::DiscoveryManager& discoveryManager()
        {
            return syncEngine().discoveryManager();
        }

        const nx::cloud::discovery::DiscoveryClient* discoveryClient()
        {
            return discoveryManager().discoveryClient();
        }

        nx::network::SocketAddress endpoint() const
        {
            auto url = syncEngineUrl();
            return nx::network::SocketAddress(url.host(), url.port());
        }

        nx::cloud::discovery::Node toNode()
        {
            nx::cloud::discovery::Node node;
            node.nodeId = m_peer.nodeId();
            return node;
        }

        Peer& peer()
        {
            return m_peer;
        }

        std::string clusterId() const
        {
            return m_peer.process().moduleInstance()->clusterId();
        }

    private:
        Peer& m_peer;
    };

public:
    std::string clusterId() const
    {
        return m_fixture.clusterId();
    }

    void addNodeContext(const nx::utils::Url& discoveryServiceUrl)
    {
        Peer& peer = m_fixture.addPeer(/*startAndWaitUntilStarted*/ false);

        peer.process().addArg("-p2pDb/discovery/enabled", "true");
        peer.process().addArg(
            "-p2pDb/discovery/discoveryServiceUrl",
            discoveryServiceUrl.toStdString().c_str());
        peer.process().addArg("-p2pDb/discovery/roundTripPadding", "2ms");
        peer.process().addArg("-p2pDb/discovery/registrationErrorDelay", "10ms");
        peer.process().addArg("-p2pDb/discovery/onlineNodesRequestDelay", "2ms");
        peer.process().addArg("-p2pDb/nodeConnectRetryTimeout", "100ms");

        ASSERT_TRUE(peer.process().startAndWaitUntilStarted());

        m_nodes.emplace_back(peer);
    }

    NodeContext& nodeContext(int index)
    {
        return m_nodes[index];
    }

private:
    ClusterTestFixture m_fixture;
    std::vector<NodeContext> m_nodes;
};

//-------------------------------------------------------------------------------------------------
// DiscoveryManager

class DiscoveryManager: public testing::Test
{
protected:
    void SetUp() override
    {
        m_server = std::make_unique<nx::cloud::discovery::test::DiscoveryServer>();

        ASSERT_TRUE(m_server->bindAndListen());
    }

    void givenOneNode()
    {
        m_fixture.addNodeContext(m_server->url());
    }

    void givenTwoNodes()
    {
        for (int i = 0; i < 2; ++i)
            m_fixture.addNodeContext(m_server->url());
    }

    void givenTwoConnectedNodes()
    {
        givenTwoNodes();
        thenNodesAreConnected();
        andSyncEnginesAreConnected();
        andSyncEnginesAreSynchronized();
    }

    void whenDiscoveryServiceStops()
    {
        m_server.reset();
    }

    void thenSyncEnginesStayConnected()
    {
        // Technically there is no way to know that nodes will stay connected if the discovery
        // server goes down. All we can do is wait and see if nodes continue to synchronize
        // new data if it is added after the discovery service stops. Let's wait a bit before
        // trying to synchronize
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // add two customers and check synchronization
        for (int i = 0; i < 2; ++i)
            andSyncEnginesAreSynchronized();
    }

    void thenNodeIsRegistered(int index = 0)
    {
        auto nodeContext = m_fixture.nodeContext(index);

        ASSERT_NE(nodeContext.discoveryClient(), nullptr);

        nx::cloud::discovery::Node node = nodeContext.discoveryClient()->node();
        while (node.nodeId != nodeContext.peer().nodeId())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            node = nodeContext.discoveryClient()->node();
        }
    }

    void thenNodesAreConnected()
    {
        auto& a = m_fixture.nodeContext(0);
        auto& b = m_fixture.nodeContext(1);

        waitForDiscovery(a, b);
        waitForDiscovery(b, a);
    }

    void andSyncEnginesAreConnected()
    {
        // Checking that one node is explicitely connected to another one.

        auto& a = m_fixture.nodeContext(0);
        auto& b = m_fixture.nodeContext(1);

        while (!a.peer().isConnectedTo(b.peer())
            && !b.peer().isConnectedTo(a.peer()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void andSyncEnginesAreSynchronized()
    {
        // Add random data to "a" and wait for "b" to synchronize it.

        auto& a = m_fixture.nodeContext(0);
        auto& b = m_fixture.nodeContext(1);

        m_expectedData.emplace_back(a.peer().addRandomData());

        while (!b.peer().hasData(m_expectedData))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

private:
    void waitForDiscovery(
        DiscoveryTestFixture::NodeContext& a,
        DiscoveryTestFixture::NodeContext& b)
    {
        // Verify that the node represented by "b" is in the online nodes list of "a".
        auto onlineNodes = a.discoveryClient()->onlineNodes();
        nx::cloud::discovery::Node bNode = b.toNode();
        while (std::find(onlineNodes.begin(), onlineNodes.end(), bNode) == onlineNodes.end())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            onlineNodes = a.discoveryClient()->onlineNodes();
        }
    }

private:
    std::unique_ptr<nx::cloud::discovery::test::DiscoveryServer> m_server;
    DiscoveryTestFixture m_fixture;
    std::vector<Customer> m_expectedData;
};

TEST_F(DiscoveryManager, discovery_service_starts)
{
    givenOneNode();

    thenNodeIsRegistered();
}

TEST_F(DiscoveryManager, discovers_other_nodes)
{
    givenTwoNodes();

    thenNodesAreConnected();

    andSyncEnginesAreConnected();
    andSyncEnginesAreSynchronized();
}

TEST_F(DiscoveryManager, sync_engines_stay_connected_if_discovery_service_stops)
{
    givenTwoConnectedNodes();

    whenDiscoveryServiceStops();

    thenSyncEnginesStayConnected();
}

} // namespace nx::clusterdb::engine::test