/**********************************************************
* Jul 26, 2016
* akolesnikov
***********************************************************/

#include "cross_nat_connector_test.h"

#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/utils/random.h>

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
    boost::optional<SocketAddress> mediatorAddressForConnector,
    std::function<void(nx::hpm::MediaServerEmulator*)> serverConfig)
{
    ConnectResult connectResult;
    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);
    if (serverConfig)
        serverConfig(server1.get());
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
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(), //< No timeout.
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

        // Implying random delay.
        std::this_thread::sleep_for(std::chrono::microseconds(
            nx::utils::random::number(0, 0xFFFF) * 10));
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
        {
            return actionOnConnectAckResponse;
        });

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen());

    nx::utils::promise<ConnectResult> connectedPromise;
    CrossNatConnector connector(
        SocketAddress((server->serverId() + "." + system.id).constData()),
        mediatorAddressForConnector);
    auto connectorGuard = makeScopedGuard([&connector]() { connector.pleaseStopSync(); });

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
    const auto actualConnectTimeout =
        connectTimeout == std::chrono::milliseconds::zero()
        ? kDefaultTestTimeout
        : (connectTimeout * 2);
    ASSERT_EQ(std::future_status::ready, connectedFuture.wait_for(actualConnectTimeout));
    *connectResult = connectedFuture.get();
    connectResult->executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);
}


class CrossNatConnector:
    public TunnelConnector
{
};

TEST_F(CrossNatConnector, timeout)
{
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const std::chrono::milliseconds connectTimeout(nx::utils::random::number(1000, 4000));

    // Timing out mediator response by providing incorrect mediator address to connector.
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
