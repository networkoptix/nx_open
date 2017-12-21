#include <boost/optional.hpp>
#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/network/cloud/tunnel/udp/connector.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

#include "cross_nat_connector_test.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

using nx::hpm::MediaServerEmulator;

constexpr const std::chrono::seconds kDefaultTestTimeout = std::chrono::seconds(15);

class UdpTunnelConnector:
    public cloud::test::TunnelConnector
{
public:
    UdpTunnelConnector():
        m_cloudConnectMaskBak(ConnectorFactory::getEnabledCloudConnectMask())
    {
        ConnectorFactory::setEnabledCloudConnectMask((int)ConnectType::udpHp);
    }

    ~UdpTunnelConnector()
    {
        ConnectorFactory::setEnabledCloudConnectMask(m_cloudConnectMaskBak);
    }

private:
    int m_cloudConnectMaskBak;
};

TEST_F(UdpTunnelConnector, general)
{
    generalTest();
}

TEST_F(UdpTunnelConnector, noSynAck)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds(5),
        MediaServerEmulator::ActionToTake::ignoreSyn);

    ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(UdpTunnelConnector, badSynAck)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(), //< No timeout.
        MediaServerEmulator::ActionToTake::sendBadSynAck);

    ASSERT_EQ(SystemError::connectionReset, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

// Currently, this test requires hack in UnreliableMessagePipeline::messageSent:
// errorCode has to be set to SystemError::connectionReset.
TEST_F(UdpTunnelConnector, DISABLED_remotePeerUdpPortNotAccessible)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(), //< No timeout.
        MediaServerEmulator::ActionToTake::proceedWithConnection);

    ASSERT_EQ(SystemError::connectionReset, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(UdpTunnelConnector, cancellation)
{
    cancellationTest();
}

TEST_F(UdpTunnelConnector, timeout)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const std::chrono::milliseconds connectTimeout(utils::random::number(1000, 4000));

    // Timing out udt connection...
    boost::optional<SocketAddress> mediatorAddressForConnector;

    const auto connectResult = doSimpleConnectTest(
        connectTimeout,
        MediaServerEmulator::ActionToTake::ignoreIndication,
        boost::none);

    ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
    //ASSERT_TRUE(
    //    connectResult.executionTime > connectTimeout*0.8 &&
    //    connectResult.executionTime < connectTimeout*1.2);
}

/** problem: server peer does not see connecting peer 
    on the same address that mediator sees connecting peer
*/
TEST_F(UdpTunnelConnector, connecting_peer_in_the_same_lan_as_mediator)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);
    //const auto connectTimeout = std::chrono::seconds(10);
    const auto connectTimeout = std::chrono::seconds::zero();

    nx::utils::promise<hpm::api::ConnectionRequestedEvent> connectionRequestedPromise;

    //starting mediaserver emulator with specified host name
    server1->setOnConnectionRequestedHandler(
        [&connectionRequestedPromise](hpm::api::ConnectionRequestedEvent eventData)
            -> MediaServerEmulator::ActionToTake
        {
            connectionRequestedPromise.set_value(eventData);
            return MediaServerEmulator::ActionToTake::proceedWithConnection;
        });
    server1->setConnectionAckResponseHandler(
        [](hpm::api::ResultCode /*resultCode*/)
            -> MediaServerEmulator::ActionToTake
        {
            return MediaServerEmulator::ActionToTake::proceedWithConnection;
        });

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server1->listen().first);

    nx::utils::promise<ConnectResult> connectedPromise;
    CrossNatConnector connector(
        SocketAddress((server1->serverId() + "." + system1.id).constData()));
    connector.replaceOriginatingHostAddress("192.168.0.1");

    connector.connect(
        connectTimeout,
        [&connectedPromise](
            SystemError::ErrorCode /*errorCode*/,
            std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)
        {
        });

    auto connectionRequestedFuture = connectionRequestedPromise.get_future();
    ASSERT_EQ(
        std::future_status::ready,
        connectionRequestedFuture.wait_for(std::chrono::seconds(7)));

    const auto connectionRequestedEvent = connectionRequestedFuture.get();
    ASSERT_TRUE(
        std::find(
            connectionRequestedEvent.udpEndpointList.begin(),
            connectionRequestedEvent.udpEndpointList.end(),
            SocketAddress(HostAddress("192.168.0.1"), connector.localAddress().port)) !=
        connectionRequestedEvent.udpEndpointList.end());

    connector.pleaseStopSync();
}

}   //namespace test
}   //namespace udp
}   //namespace cloud
}   //namespace network
}   //namespace nx
