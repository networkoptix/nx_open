#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/direct_endpoint_connector.h>

#include "cross_nat_connector_test.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

using nx::hpm::MediaServerEmulator;

class TcpTunnelConnector
:
    public cloud::test::TunnelConnector
{
public:
    TcpTunnelConnector()
    {
        //disabling udp hole punching and enabling tcp port forwarding
        ConnectorFactory::setEnabledCloudConnectMask(
            (int)CloudConnectType::forwardedTcpPort);
    }

    ~TcpTunnelConnector()
    {
        ConnectorFactory::setEnabledCloudConnectMask(
            (int)CloudConnectType::all);
    }
};

TEST_F(TcpTunnelConnector, general)
{
    generalTest();
}

TEST_F(TcpTunnelConnector, failModuleInformation)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds(5),
        MediaServerEmulator::ActionToTake::proceedWithConnection,
        boost::none,
        [](MediaServerEmulator* server)
        {
            server->setServerIdForModuleInformation(boost::none);
        });

    ASSERT_EQ(SystemError::connectionReset, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(TcpTunnelConnector, cancellation)
{
    cancellationTest();
}

// Checking it does not connect to a server which does not provide 
//  cloudSystemId in /api/moduleInformation response.
TEST_F(TcpTunnelConnector, connectedToWrongServer)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto system1 = mediator().addRandomSystem();

    struct TestData
    {
        boost::optional<nx::String> token;
        bool isCorrect;
    };

    const TestData testSystemIdArray[] = {
        {system1.id, true},
        {nx::String("invalid_cloud_system_id"), false},
        {boost::none, false} };

    // Connecting to a specific server within a system, 
    //  but connected to another server of that system.

    const auto peerId = QnUuid::createUuid();
    const TestData testPeerIdArray[] = {
        { peerId.toByteArray(), true },
        { QnUuid::createUuid().toByteArray(), false },
        { boost::none, false } };

    for (size_t i = 0; i < sizeof(testSystemIdArray) / sizeof(*testSystemIdArray); ++i)
    {
        const auto& systemIdTestContext = testSystemIdArray[i];

        for (size_t j = 0; j < sizeof(testPeerIdArray) / sizeof(*testPeerIdArray); ++j)
        {
            const auto& peerIdTestContext = testPeerIdArray[j];

            std::cout<<"test debug: "<<i<<", "<<j<<std::endl;

            ConnectResult connectResult;
            auto server1 = mediator().addRandomServer(system1, peerId);

            server1->setCloudSystemIdForModuleInformation(systemIdTestContext.token);
            server1->setServerIdForModuleInformation(peerIdTestContext.token);
            doSimpleConnectTest(
                std::chrono::seconds::zero(),   //no timeout
                MediaServerEmulator::ActionToTake::proceedWithConnection,
                system1,
                server1,
                boost::none,
                &connectResult);

            if (systemIdTestContext.isCorrect && peerIdTestContext.isCorrect)
            {
                ASSERT_EQ(SystemError::noError, connectResult.errorCode);
                ASSERT_NE(nullptr, connectResult.connection);
                connectResult.connection->pleaseStopSync();
            }
            else
            {
                ASSERT_NE(SystemError::noError, connectResult.errorCode);
                ASSERT_EQ(nullptr, connectResult.connection);
            }
        }
    }
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
