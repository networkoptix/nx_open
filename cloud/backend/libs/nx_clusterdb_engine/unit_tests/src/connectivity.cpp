#include <gtest/gtest.h>

#include <nx/clusterdb/engine/transport/transport_manager.h>
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
    virtual void SetUp() override
    {
        for (int i = 0; i < 2; ++i)
        {
            auto& peer = addPeer();

            nx::utils::SubscriptionId subscriptionId = nx::utils::kInvalidSubscriptionId;
            peer.process().moduleInstance()->synchronizationEngine()
                .connectionManager().nodeStatusChangedSubscription().subscribe(
                    [this](auto&&... args) { saveEvent(std::forward<decltype(args)>(args)...); },
                    &subscriptionId);
            m_nodeSubscriptions.push_back({&peer, subscriptionId});
        }
    }

    virtual void TearDown() override
    {
        for (auto& [peer, subscriptionId]: m_nodeSubscriptions)
        {
            peer->process().moduleInstance()->synchronizationEngine()
                .connectionManager().nodeStatusChangedSubscription().removeSubscription(
                    subscriptionId);
        }
    }

    void givenConnectedNodes()
    {
        whenConnectNodes();
        thenPeerConnectedEventIsDelivered({peer(0).nodeId(), peer(1).nodeId()});
    }

    void whenConnectNodes()
    {
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

private:
    using ConnectResult = std::tuple<
        transport::ConnectResultDescriptor,
        std::unique_ptr<transport::AbstractConnection>>;

    std::vector<std::tuple<Peer*, nx::utils::SubscriptionId>> m_nodeSubscriptions;
    nx::utils::SyncQueue<std::tuple<NodeDescriptor, NodeStatusDescriptor>> m_events;
    std::unique_ptr<transport::AbstractTransactionTransportConnector> m_connector;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;

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
            ASSERT_EQ(1, nodeIds.end() - removedNodeIter);

            nodeIds.erase(removedNodeIter, nodeIds.end());

            if (nodeIds.empty())
                break;
        }
    }

    void saveConnectResult(
        transport::ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<transport::AbstractConnection> connection)
    {
        m_connectResults.push({
            std::move(connectResultDescriptor),
            std::move(connection)});
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

} // namespace nx::clusterdb::engine::test
