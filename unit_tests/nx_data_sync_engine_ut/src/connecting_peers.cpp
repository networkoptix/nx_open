#include "cluster_test_fixture.h"

#include <nx/utils/random.h>

namespace nx::data_sync_engine::test {

class ConnectingPeers:
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

TEST_F(ConnectingPeers, DISABLED_peer_can_be_explicitly_connected_to_another_one)
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

} // namespace nx::data_sync_engine::test
