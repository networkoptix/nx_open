#include <gtest/gtest.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

namespace {

static constexpr char kClusterId[] = "DiscoveryManagerTestCluster";

} // namespace

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
            node.nodeId = nodeId();
            return node;
        }

        std::string nodeId() const
        {
            return m_peer.nodeId();
        }

    private:
        Peer& m_peer;
    };

public:
    void addNodeContext(const nx::utils::Url& discoveryServiceUrl)
    {
        const auto discoveryServiceUrlArg = lm("--discovery/discoveryServiceUrl=%1")
            .arg(discoveryServiceUrl.toString());

        // 2 milliseconds padding for testing purposes
        const auto roundTripPaddingArg = lm("--discovery/roundTripPadding=%1")
            .arg(QString::number(2));

        const auto registrationErrorDelayArg = lm("--discovery/registrationErrorDelay=%1")
            .arg(100);

        const auto onlineNodesRequestDelayArg = lm("--discovery/onlineNodesRequestDelay=%1")
            .arg(QString::number(2));

        m_fixture.addPeer(/*startAndWaitUntilStarted*/ false);
        Peer& peer = m_fixture.peer(m_fixture.peerCount() - 1);

        peer.process().addArg(discoveryServiceUrlArg.toUtf8().constData());
        peer.process().addArg(roundTripPaddingArg.toUtf8().constData());
        peer.process().addArg(registrationErrorDelayArg.toUtf8().constData());
        peer.process().addArg(onlineNodesRequestDelayArg.toUtf8().constData());

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
        m_server = std::make_unique<nx::cloud::discovery::test::DiscoveryServer>(kClusterId);
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

    void givenRegisteredNode(int index = 0)
    {
        givenOneNode();
        whenNodeStartsDiscovery(index);
        thenNodeIsRegistered(index);
    }

    void whenBothNodesStartDiscovery()
    {
        for (int i = 0; i < 2; ++i)
            whenNodeStartsDiscovery(i);
    }

    void whenNodeStartsDiscovery(int index = 0)
    {
        auto& nodeContext = m_fixture.nodeContext(index);
        nodeContext.discoveryManager().start(
            kClusterId,
            nodeContext.syncEngineUrl());
    }

    void thenNodeIsRegistered(int index = 0)
    {
        auto nodeContext = m_fixture.nodeContext(index);

        ASSERT_NE(nodeContext.discoveryClient(), nullptr);

        nx::cloud::discovery::Node node = nodeContext.discoveryClient()->node();
        while (node.nodeId != nodeContext.nodeId())
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
        auto& a = m_fixture.nodeContext(0);
        auto& b = m_fixture.nodeContext(1);

        waitForSyncEngineConnection(a, b);
        waitForSyncEngineConnection(b, a);
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

    void waitForSyncEngineConnection(
        DiscoveryTestFixture::NodeContext& a,
        DiscoveryTestFixture::NodeContext& b)
    {
        // Verify that the sync engine of "b" is in the list of connections of "a"
        const auto bIsEqual =
            [&b](const engine::SystemConnectionInfo& other)
            {
                return b.endpoint() == other.peerEndpoint;
            };

        auto connections = a.syncEngine().connectionManager().getConnections();
        while (std::find_if(connections.begin(), connections.end(), bIsEqual) == connections.end())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            connections = a.syncEngine().connectionManager().getConnections();
        }
    }

private:
    std::unique_ptr<nx::cloud::discovery::test::DiscoveryServer> m_server;
    DiscoveryTestFixture m_fixture;
};

TEST_F(DiscoveryManager, discovery_service_starts)
{
    givenOneNode();

    whenNodeStartsDiscovery();

    thenNodeIsRegistered();
}

TEST_F(DiscoveryManager, discovers_other_nodes)
{
    givenTwoNodes();

    whenBothNodesStartDiscovery();

    thenNodesAreConnected();

    andSyncEnginesAreConnected();
}

} // namespace nx::clusterdb::engine::test