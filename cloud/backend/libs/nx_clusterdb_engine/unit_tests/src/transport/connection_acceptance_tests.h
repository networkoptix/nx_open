#pragma once

#include <thread>

#include <gtest/gtest.h>

#include <nx/clusterdb/engine/transport/connector_factory.h>

#include "../cluster_test_fixture.h"

namespace nx::clusterdb::engine::transport::test {

template<typename ConnectorTypeInstaller>
class ConnectionAcceptance:
    public ::testing::Test,
    public engine::test::ClusterTestFixture
{
public:
    ConnectionAcceptance()
    {
        m_factoryFunctionBak =
            ConnectorTypeInstaller::configureFactory(&ConnectorFactory::instance());
    }

    ~ConnectionAcceptance()
    {
        ConnectorFactory::instance().setCustomFunc(
            std::exchange(m_factoryFunctionBak, nullptr));
    }

protected:
    virtual void SetUp() override
    {
        for (int i = 0; i < 2; ++i)
        {
            addPeer();
            peer(i).addRandomData();
        }
    }

    void addRandomDataToPeerAsync(int peerNumber)
    {
        m_asyncDataGenerationFuture = std::async(
            [this, peerNumber]()
            {
                for (int i = 0; i < 21; ++i)
                    peer(peerNumber).addRandomData();
            });
    }

    void waitForAsyncDataAddingCompletion()
    {
        if (m_asyncDataGenerationFuture)
        {
            m_asyncDataGenerationFuture->wait();
            m_asyncDataGenerationFuture = std::nullopt;
        }
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

private:
    ConnectorFactory::Function m_factoryFunctionBak;
    std::optional<std::future<void>> m_asyncDataGenerationFuture;
};

TYPED_TEST_CASE_P(ConnectionAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(ConnectionAcceptance, data_can_be_exchanged_over_connection)
{
    this->whenConnectPeers();

    this->thenPeersSynchronizedEventually();
}

TYPED_TEST_P(ConnectionAcceptance, data_generated_simultaneously_with_connect_does_not_get_lost)
{
    this->addRandomDataToPeerAsync(0);
    this->whenConnectPeers();
    this->waitForAsyncDataAddingCompletion();

    this->thenPeersSynchronizedEventually();
}

REGISTER_TYPED_TEST_CASE_P(ConnectionAcceptance,
    data_can_be_exchanged_over_connection,
    data_generated_simultaneously_with_connect_does_not_get_lost
);

} // namespace nx::clusterdb::engine::transport::test
