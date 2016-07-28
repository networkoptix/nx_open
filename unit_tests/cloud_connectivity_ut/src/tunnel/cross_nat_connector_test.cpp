/**********************************************************
* Jul 26, 2016
* akolesnikov
***********************************************************/

#include "cross_nat_connector_test.h"

#include <nx/network/cloud/tunnel/cross_nat_connector.h>


namespace nx {
namespace network {
namespace cloud {
namespace test {

using nx::hpm::MediaServerEmulator;

constexpr const std::chrono::seconds kDefaultTestTimeout = std::chrono::seconds(15);

TunnelConnector::~TunnelConnector()
{
    if (m_oldFactoryFunc)
        ConnectorFactory::setFactoryFunc(std::move(*m_oldFactoryFunc));
}

void TunnelConnector::setConnectorFactoryFunc(ConnectorFactory::FactoryFunc newFactoryFunc)
{
    auto oldFunc = ConnectorFactory::setFactoryFunc(std::move(newFactoryFunc));
    if (!m_oldFactoryFunc)
        m_oldFactoryFunc = oldFunc;
}

const hpm::MediatorFunctionalTest& TunnelConnector::mediator() const
{
    return m_mediator;
}

hpm::MediatorFunctionalTest& TunnelConnector::mediator()
{
    return m_mediator;
}

TunnelConnector::ConnectResult TunnelConnector::doSimpleConnectTest(
    std::chrono::milliseconds connectTimeout,
    MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
    boost::optional<SocketAddress> mediatorAddressForConnector)
{
    ConnectResult connectResult;
    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);
    doSimpleConnectTest(
        connectTimeout,
        actionOnConnectAckResponse,
        system1,
        server1,
        mediatorAddressForConnector,
        &connectResult);
    return connectResult;
}

TunnelConnector::ConnectResult TunnelConnector::doSimpleConnectTest(
    std::chrono::milliseconds connectTimeout,
    MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
    const nx::hpm::AbstractCloudDataProvider::System& system,
    const std::unique_ptr<MediaServerEmulator>& server,
    boost::optional<SocketAddress> mediatorAddressForConnector)
{
    ConnectResult connectResult;
    doSimpleConnectTest(
        connectTimeout,
        actionOnConnectAckResponse,
        system,
        server,
        mediatorAddressForConnector,
        &connectResult);
    return connectResult;
}

void TunnelConnector::generalTest()
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(),   //no timeout
        MediaServerEmulator::ActionToTake::proceedWithConnection);

    ASSERT_EQ(SystemError::noError, connectResult.errorCode);
    ASSERT_NE(nullptr, connectResult.connection);

    connectResult.connection->pleaseStopSync();
}

void TunnelConnector::cancellationTest()
{
    const std::chrono::seconds totalTestTime(10);

    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto system = mediator().addRandomSystem();
    const auto server = mediator().addRandomServer(system);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen());

    const auto t1 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t1 < totalTestTime)
    {
        CrossNatConnector connector(
            SocketAddress((server->serverId() + "." + system.id).constData()));

        connector.connect(
            std::chrono::milliseconds::zero(),
            [](SystemError::ErrorCode /*errorCode*/,
                std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)
        {
        });

        //implying random delay
        std::this_thread::sleep_for(std::chrono::microseconds(rand() & 0xffff) * 10);

        connector.pleaseStopSync();
    }
}

void TunnelConnector::doSimpleConnectTest(
    std::chrono::milliseconds connectTimeout,
    MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
    const nx::hpm::AbstractCloudDataProvider::System& system,
    const std::unique_ptr<MediaServerEmulator>& server,
    boost::optional<SocketAddress> mediatorAddressForConnector,
    ConnectResult* const connectResult)
{
    //starting mediaserver emulator with specified host name
    server->setConnectionAckResponseHandler(
        [actionOnConnectAckResponse](nx::hpm::api::ResultCode /*resultCode*/)
        -> MediaServerEmulator::ActionToTake
    {
        return actionOnConnectAckResponse;
    });

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen());

    nx::utils::promise<ConnectResult> connectedPromise;
    CrossNatConnector connector(
        SocketAddress((server->serverId() + "." + system.id).constData()),
        mediatorAddressForConnector);

    auto t1 = std::chrono::steady_clock::now();
    connector.connect(
        connectTimeout,
        [&connectedPromise](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
        {
            ConnectResult result;
            result.errorCode = errorCode;
            result.connection = std::move(connection);
            connectedPromise.set_value(std::move(result));
        });
    auto connectedFuture = connectedPromise.get_future();
    ASSERT_EQ(
        std::future_status::ready,
        connectedFuture.wait_for(
            connectTimeout == std::chrono::milliseconds::zero()
            ? kDefaultTestTimeout
            : connectTimeout * 2));
    *connectResult = connectedFuture.get();
    connectResult->executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);

    connector.pleaseStopSync();
}


class CrossNatConnector
    :
    public TunnelConnector
{
};

TEST_F(CrossNatConnector, timeout)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const std::chrono::milliseconds connectTimeout(1000 + (rand() % 3000));

    //timing out mediator response by providing incorrect mediator address to connector
    const auto connectResult = doSimpleConnectTest(
        connectTimeout,
        MediaServerEmulator::ActionToTake::ignoreIndication,
        SocketAddress(HostAddress::localhost, 10345));

    ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
    //ASSERT_TRUE(
    //    connectResult.executionTime > connectTimeout*0.8 &&
    //    connectResult.executionTime < connectTimeout*1.2);
}

TEST_F(CrossNatConnector, target_host_not_found)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);

    auto incorrectSystem = system1;
    incorrectSystem.id += "_hren";

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(),   //no timeout
        MediaServerEmulator::ActionToTake::proceedWithConnection,
        incorrectSystem,
        server1);

    ASSERT_EQ(SystemError::hostNotFound, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

}   //namespace test
}   //namespace cloud
}   //namespace network
}   //namespace nx
