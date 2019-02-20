#pragma once

#include <type_traits>

#include <nx/clusterdb/engine/transport/connector_factory.h>

#include <nx/utils/random.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

template<typename ConnectorTypeInstaller>
class Synchronization:
    public ::testing::Test,
    public ClusterTestFixture
{
public:
    Synchronization()
    {
        m_factoryFunctionBak =
            ConnectorTypeInstaller::configureFactory(&transport::ConnectorFactory::instance());
    }

    ~Synchronization()
    {
        transport::ConnectorFactory::instance().setCustomFunc(
            std::exchange(m_factoryFunctionBak, nullptr));
    }

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

private:
    transport::ConnectorFactory::Function m_factoryFunctionBak;
};

TYPED_TEST_CASE_P(Synchronization);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(Synchronization, peer_can_be_explicitly_connected_to_another_one)
{
    this->givenPeersOfSameApplication(2);

    this->whenAddDataToEveryPeer();
    this->whenConnectPeers();

    this->thenPeersSynchronizedEventually();
}

TYPED_TEST_P(Synchronization, DISABLED_connected_peers_exchange_data)
{
    this->givenConnectedPeers();
    this->whenAddDataToRandomPeer();
    this->thenPeersSynchronizedEventually();
}

TYPED_TEST_P(Synchronization, peers_connected_as_a_chain_are_synchronized)
{
    this->givenPeersOfSameApplication(3);

    this->whenAddDataToEveryPeer();
    this->whenChainPeers();

    this->thenPeersSynchronizedEventually();
}

TYPED_TEST_P(Synchronization, with_outgoing_command_filter)
{
    this->givenSynchronizedPeers(3);

    this->peer(0).process().stop();

    this->peer(2).addRandomData();
    this->thenPeersSynchronizedEventually({1, 2});
    this->peer(2).process().stop();
    // NOTE: At this point peer 1 contains data from peer 2. This data is missing on peer 0.

    // Using filter we forbid peer1 to synchronize peer2 data to peer0.
    this->peer(1).setOutgoingCommandFilter(OutgoingCommandFilterConfiguration{true});
    ASSERT_TRUE(this->peer(0).process().startAndWaitUntilStarted());
    this->peer(1).connectTo(this->peer(0));

    // NOTE: Peers 0 and 1 will never be fully synchronized since data from peer 2 will never
    // be sent from peer 1 to peer 0.
    this->thenPeersAreAbleToSynchronizeNewData({0, 1});
}

TYPED_TEST_P(Synchronization, peer_can_receive_same_data_from_multiple_peers_simultaneously)
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

    this->givenSynchronizedPeers(count);
    this->peer(0).process().stop();
    this->peer(1).process().stop();

    for (int i = 2; i < this->peerCount(); ++i)
    {
        for(int j = 0; j < 17; ++j)
            this->peer(i).addRandomData();
    }
    this->thenPeersSynchronizedEventually({2, 3});

    ASSERT_TRUE(this->peer(0).process().startAndWaitUntilStarted());
    ASSERT_TRUE(this->peer(1).process().startAndWaitUntilStarted());
    this->peer(0).connectTo(this->peer(1));
    for (int i = 2; i < this->peerCount(); ++i)
        this->peer(1).connectTo(this->peer(i));

    this->thenPeersSynchronizedEventually();
}


REGISTER_TYPED_TEST_CASE_P(Synchronization,
    peer_can_be_explicitly_connected_to_another_one,
    DISABLED_connected_peers_exchange_data,
    peers_connected_as_a_chain_are_synchronized,
    with_outgoing_command_filter,
    peer_can_receive_same_data_from_multiple_peers_simultaneously
);

} // namespace nx::clusterdb::engine::test
