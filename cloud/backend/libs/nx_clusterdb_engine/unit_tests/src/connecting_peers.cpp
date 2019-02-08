#include <nx/utils/random.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

class ConnectingPeers:
    public ::testing::Test,
    public ClusterTestFixture
{
protected:
    void givenTwoPeersOfSameApplication()
    {
        for (int i = 0; i < 2; ++i)
            addPeer();
    }

    void givenConnectedPeers()
    {
        givenTwoPeersOfSameApplication();
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
        peer(1).connectTo(peer(0));
    }

    void thenPeersSynchronizedEventually()
    {
        while (!allPeersAreSynchronized())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

TEST_F(ConnectingPeers, peer_can_be_explicitly_connected_to_another_one)
{
    givenTwoPeersOfSameApplication();

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

} // namespace nx::clusterdb::engine::test
