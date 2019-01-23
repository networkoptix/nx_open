#pragma once

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
};

TYPED_TEST_CASE_P(ConnectionAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(ConnectionAcceptance, data_can_be_exchanged_over_connection)
{
    whenConnectPeers();

    thenPeersSynchronizedEventually();
}

REGISTER_TYPED_TEST_CASE_P(ConnectionAcceptance,
    data_can_be_exchanged_over_connection
);

} // namespace nx::clusterdb::engine::transport::test
