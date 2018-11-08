#include "cross_nat_connector_test.h"

#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

using nx::hpm::MediaServerEmulator;

constexpr const std::chrono::seconds kDefaultTestTimeout = std::chrono::seconds(15);

TunnelConnector::~TunnelConnector()
{
    if (m_oldFactoryFunc)
    {
        CrossNatConnectorFactory::instance().setCustomFunc(
            std::move(*m_oldFactoryFunc));
    }
}

void TunnelConnector::setConnectorFactoryFunc(
    CrossNatConnectorFactory::Function newFactoryFunc)
{
    auto oldFunc =
        CrossNatConnectorFactory::instance().setCustomFunc(
            std::move(newFactoryFunc));
    if (!m_oldFactoryFunc)
        m_oldFactoryFunc = std::move(oldFunc);
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
    std::optional<hpm::api::MediatorAddress> mediatorAddress,
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
        mediatorAddress,
        &connectResult);
    return connectResult;
}

TunnelConnector::ConnectResult TunnelConnector::doSimpleConnectTest(
    std::chrono::milliseconds connectTimeout,
    MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
    const nx::hpm::AbstractCloudDataProvider::System& system,
    const std::unique_ptr<MediaServerEmulator>& server,
    std::optional<hpm::api::MediatorAddress> mediatorAddress)
{
    ConnectResult connectResult;
    doSimpleConnectTest(
        connectTimeout,
        actionOnConnectAckResponse,
        system,
        server,
        mediatorAddress,
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
    ASSERT_NE(nullptr, server);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen().first);

    const auto t1 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t1 < totalTestTime)
    {
        CrossNatConnector connector(
            &SocketGlobals::cloud(),
            nx::network::SocketAddress((server->serverId() + "." + system.id).constData()));

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
    std::optional<hpm::api::MediatorAddress> mediatorAddress,
    ConnectResult* const connectResult)
{
    //starting mediaserver emulator with specified host name
    server->setConnectionAckResponseHandler(
        [actionOnConnectAckResponse](nx::hpm::api::ResultCode /*resultCode*/)
        {
            return actionOnConnectAckResponse;
        });

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen().first);

    nx::utils::promise<ConnectResult> connectedPromise;
    CrossNatConnector connector(
        &SocketGlobals::cloud(),
        nx::network::SocketAddress((server->serverId() + "." + system.id).constData()),
        mediatorAddress);
    auto connectorGuard = nx::utils::makeScopeGuard(
        [&connector]() { connector.pleaseStopSync(); });

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

    *connectResult = connectedPromise.get_future().get();
    connectResult->executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);
}

}   //namespace test
}   //namespace cloud
}   //namespace network
}   //namespace nx
