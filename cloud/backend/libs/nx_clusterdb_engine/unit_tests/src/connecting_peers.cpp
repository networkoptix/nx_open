#include <type_traits>

#include <nx/utils/random.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

class ConnectingPeers:
    public ::testing::Test,
    public ClusterTestFixture
{
protected:
    void givenPeersOfSameApplication(int count)
    {
        for (int i = 0; i < count; ++i)
            addPeer();
    }

    void givenConnectedPeers()
    {
        givenPeersOfSameApplication(2);
        whenConnectPeers();
        thenPeersSynchronizedEventually();
    }

    void givenSynchronizedPeers(int count)
    {
        givenPeersOfSameApplication(count);

        whenAddDataToEveryPeer();
        whenConnectPeers();

        thenPeersSynchronizedEventually();
    }

    void whenAddDataToRandomPeer()
    {
        peer(nx::utils::random::number<int>(0, peerCount() - 1)).addRandomData();
    }

    void whenAddDataToEveryPeer()
    {
        for (int i = 0; i < peerCount(); ++i)
            peer(i).addRandomData();
    }

    void whenConnectPeers()
    {
        // TODO: Connect "each to each" or generate random connected graph.
        whenChainPeers();
    }

    void whenChainPeers()
    {
        for (int i = 0; i < peerCount() - 1; ++i)
            peer(i + 1).connectTo(peer(i));
    }

    void thenPeersSynchronizedEventually()
    {
        while (!allPeersAreSynchronized())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeersSynchronizedEventually(const std::vector<int>& peerIds)
    {
        while (!peersAreSynchronized(peerIds))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeersAreAbleToSynchronizeNewData(const std::vector<int>& peerIds)
    {
        std::vector<decltype(Peer().addRandomData())> newData;
        for (int id: peerIds)
            newData.push_back(peer(id).addRandomData());

        for (int id: peerIds)
        {
            while (!peer(id).hasData(newData))
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

TEST_F(ConnectingPeers, peer_can_be_explicitly_connected_to_another_one)
{
    givenPeersOfSameApplication(2);

    whenAddDataToEveryPeer();
    whenConnectPeers();

    thenPeersSynchronizedEventually();
}

TEST_F(ConnectingPeers, DISABLED_connected_peers_exchange_data)
{
    givenConnectedPeers();
    whenAddDataToRandomPeer();
    thenPeersSynchronizedEventually();
}

TEST_F(ConnectingPeers, as_a_chain)
{
    givenPeersOfSameApplication(3);

    whenAddDataToEveryPeer();
    whenChainPeers();

    thenPeersSynchronizedEventually();
}

TEST_F(ConnectingPeers, with_outgoing_command_filter)
{
    givenSynchronizedPeers(3);

    peer(0).process().stop();

    peer(2).addRandomData();
    thenPeersSynchronizedEventually({1, 2});
    peer(2).process().stop();
    // NOTE: At this point peer 1 contains data from peer 2. This data is missing on peer 0.

    // Using filter we forbid peer1 to synchronize peer2 data to peer0.
    peer(1).setOutgoingCommandFilter(OutgoingCommandFilterConfiguration{true});
    ASSERT_TRUE(peer(0).process().startAndWaitUntilStarted());
    peer(1).connectTo(peer(0));

    // NOTE: Peers 0 and 1 will never be fully synchronized since data from peer 2 will never
    // be sent from peer 1 to peer 0.
    thenPeersAreAbleToSynchronizeNewData({0, 1});
}

TEST_F(ConnectingPeers, receiving_same_data_from_multiple_peers_simultaneously)
{
    constexpr int count = 4;

    /**
     * Peer network graph:
     *   2 - 3
     *    \ /
     *     1
     *     |
     *     0
     */

    givenSynchronizedPeers(count);
    peer(0).process().stop();
    peer(1).process().stop();

    for (int i = 2; i < peerCount(); ++i)
    {
        for(int j = 0; j < 17; ++j)
            peer(i).addRandomData();
    }
    thenPeersSynchronizedEventually({2, 3});

    ASSERT_TRUE(peer(0).process().startAndWaitUntilStarted());
    ASSERT_TRUE(peer(1).process().startAndWaitUntilStarted());
    peer(0).connectTo(peer(1));
    for (int i = 2; i < peerCount(); ++i)
        peer(1).connectTo(peer(i));

    thenPeersSynchronizedEventually();
}

} // namespace nx::clusterdb::engine::test
