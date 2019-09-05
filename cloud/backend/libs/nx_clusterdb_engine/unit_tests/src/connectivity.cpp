#include <gtest/gtest.h>

#include <nx/clusterdb/engine/transport/transport_manager.h>
#include <nx/utils/math/graph.h>
#include <nx/utils/thread/sync_queue.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

class Connectivity:
    public ::testing::Test,
    public ClusterTestFixture
{
public:
    ~Connectivity()
    {
        if (m_connector)
            m_connector->pleaseStopSync();
    }

protected:
    virtual void TearDown() override
    {
        for (auto& [peer, subscriptions]: m_nodeSubscriptions)
        {
            peer->process().moduleInstance()->synchronizationEngine()
                .connectionManager().nodeStatusChangedSubscription().removeSubscription(
                    subscriptions.onStatusChanged);

            peer->process().moduleInstance()->synchronizationEngine()
                .connector().onConnectCompletedSubscription().removeSubscription(
                    subscriptions.onConnectCompletion);
        }
    }

    void addArg(std::string arg)
    {
        m_args.push_back(std::move(arg));
    }

    void givenConnectedNodes()
    {
        whenConnectNodes();
        thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
    }

    void givenNodesConnectedToEachOther()
    {
        whenConnectNodesToEachOther();

        thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
        andPeersAreConnected();
    }

    void whenConnectNodes()
    {
        if (peerCount() == 0)
            initializePeers();

        peer(0).connectTo(peer(1));
    }

    void whenDisconnectNodes()
    {
        peer(0).disconnectFrom(peer(1));
    }

    void whenEstablishDuplicateConnectionExplicitely()
    {
        m_connector = peer(0).process().moduleInstance()->synchronizationEngine()
            .transportManager().createConnector(
                clusterId(),
                "connection_id",
                peer(1).process().moduleInstance()->synchronizationUrl());

        m_connector->connect(
            [this](auto&&... args) { saveConnectResult(std::forward<decltype(args)>(args)...); });
    }

    void whenConnectNodesToEachOther()
    {
        if (peerCount() == 0)
            initializePeers();

        for (int i = 0; i < peerCount(); ++i)
        {
            for (int j = 0; j < peerCount(); ++j)
            {
                if (i != j)
                    peer(i).connectTo(peer(j));
            }
        }
    }

    void thenPeerConnectedEventIsDelivered(std::vector<std::string> nodeIds)
    {
        assertPeerEventIsDelivered(std::move(nodeIds), true);
    }

    void thenPeerDisconnectedEventIsDelivered(std::vector<std::string> nodeIds)
    {
        assertPeerEventIsDelivered(std::move(nodeIds), false);
    }

    void thenConnectionIsEstablishedSuccessfully()
    {
        auto [result, connection] = m_connectResults.pop();
        ASSERT_TRUE(result.ok());

        if (connection)
            connection->pleaseStopSync();
    }

    void thenNodesAreConnected()
    {
        thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
    }

    void andPeersAreConnected()
    {
        nx::utils::math::Graph<std::string /*nodeId*/> nodes;

        for (int i = 0; i < peerCount(); ++i)
        {
            nodes.addVertex(peer(i).nodeId());

            const auto connections = peer(i).process().moduleInstance()->synchronizationEngine()
                .connectionManager().getConnections();
            for (const auto& connection: connections)
                nodes.addEdge(peer(i).nodeId(), connection.nodeId);
        }

        ASSERT_TRUE(nodes.connected());
    }

    void andConnectivityIsReliable()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        andPeersAreConnected();
    }

    void thenConnectAttemptsAreStoppedEventually(std::chrono::milliseconds silentPeriod)
    {
        while (m_connectCompletionEvents.pop(silentPeriod)) {}
    }

private:
    using ConnectResult = std::tuple<
        transport::ConnectResult,
        std::unique_ptr<transport::AbstractConnection>>;

    using ConnectCompletionEvent =
        std::tuple<nx::utils::Url, transport::ConnectResult>;

    struct NodeScriptions
    {
        nx::utils::SubscriptionId onStatusChanged = nx::utils::kInvalidSubscriptionId;
        nx::utils::SubscriptionId onConnectCompletion = nx::utils::kInvalidSubscriptionId;
    };

    std::vector<std::tuple<Peer*, NodeScriptions>> m_nodeSubscriptions;
    nx::utils::SyncQueue<std::tuple<NodeDescriptor, NodeStatusDescriptor>> m_events;
    std::unique_ptr<transport::AbstractTransactionTransportConnector> m_connector;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    nx::utils::SyncQueue<ConnectCompletionEvent> m_connectCompletionEvents;
    std::vector<std::string> m_args;

    void initializePeers()
    {
        std::vector<std::pair<const char*, const char*>> args;
        std::transform(
            m_args.begin(), m_args.end(),
            std::back_inserter(args),
            [](const std::string& arg) { return std::make_pair(arg.c_str(), ""); });

        for (int i = 0; i < 2; ++i)
        {
            auto& peer = addPeer(args);

            nx::utils::SubscriptionId onStatusChanged = nx::utils::kInvalidSubscriptionId;
            peer.process().moduleInstance()->synchronizationEngine()
                .connectionManager().nodeStatusChangedSubscription().subscribe(
                    [this](auto&&... args) { saveEvent(std::forward<decltype(args)>(args)...); },
                    &onStatusChanged);

            nx::utils::SubscriptionId onConnectCompletion = nx::utils::kInvalidSubscriptionId;
            peer.process().moduleInstance()->synchronizationEngine()
                .connector().onConnectCompletedSubscription().subscribe(
                    [this](auto&&... args) { saveConnectCompletionEvent(std::forward<decltype(args)>(args)...); },
                    &onConnectCompletion);

            m_nodeSubscriptions.push_back({&peer, {onStatusChanged, onConnectCompletion}});
        }
    }

    void saveEvent(const NodeDescriptor& node, const NodeStatusDescriptor& nodeStatus)
    {
        m_events.push(std::make_tuple(node, nodeStatus));
    }

    void assertPeerEventIsDelivered(
        std::vector<std::string> nodeIds,
        bool expectedOnline)
    {
        for (;;)
        {
            auto[nodeDescriptor, nodeStatusDescriptor] = m_events.pop();

            ASSERT_EQ(expectedOnline, nodeStatusDescriptor.isOnline);

            const auto removedNodeIter =
                std::remove(nodeIds.begin(), nodeIds.end(), nodeDescriptor.name.peerId);
            ASSERT_NE(nodeIds.end(), removedNodeIter);
            ASSERT_EQ(1, nodeIds.end() - removedNodeIter);

            nodeIds.erase(removedNodeIter, nodeIds.end());

            if (nodeIds.empty())
                break;
        }
    }

    void saveConnectResult(
        transport::ConnectResult connectResultDescriptor,
        std::unique_ptr<transport::AbstractConnection> connection)
    {
        m_connectResults.push({
            std::move(connectResultDescriptor),
            std::move(connection)});
    }

    void saveConnectCompletionEvent(
        const nx::utils::Url& remoteNodeUrl, const transport::ConnectResult& result)
    {
        m_connectCompletionEvents.push({remoteNodeUrl, result});
    }
};

TEST_F(Connectivity, node_connected_event_is_delivered)
{
    whenConnectNodes();
    thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
}

TEST_F(Connectivity, node_disconnected_event_is_delivered)
{
    givenConnectedNodes();
    whenDisconnectNodes();
    thenPeerDisconnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
}

TEST_F(Connectivity, new_connection_from_already_connected_node_overrides_existing_one)
{
    givenConnectedNodes();
    whenEstablishDuplicateConnectionExplicitely();
    thenConnectionIsEstablishedSuccessfully();
}

TEST_F(Connectivity, nodes_can_be_connected_to_each_other_simultaneously)
{
    whenConnectNodesToEachOther();

    thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
    andPeersAreConnected();
    andConnectivityIsReliable();
}

TEST_F(Connectivity, already_connected_nodes_do_not_try_to_establish_another_connection)
{
    addArg("--p2pDb/nodeConnectRetryTimeout=100ms");

    givenNodesConnectedToEachOther();
    thenConnectAttemptsAreStoppedEventually(std::chrono::milliseconds(300));
}

} // namespace nx::clusterdb::engine::test
