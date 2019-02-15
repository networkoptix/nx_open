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

} // namespace nx::clusterdb::engine::test
