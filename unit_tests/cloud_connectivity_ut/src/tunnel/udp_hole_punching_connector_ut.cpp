/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include <future>

#include <boost/optional.hpp>
#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/udp/connector.h>
#include <nx/network/socket_global.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

using nx::hpm::MediaServerEmulator;

constexpr const std::chrono::seconds kDefaultTestTimeout = std::chrono::seconds(15);

class TunnelConnector
:
    public ::testing::Test
{
public:
    ~TunnelConnector()
    {
        if (m_oldFactoryFunc)
            ConnectorFactory::setFactoryFunc(std::move(*m_oldFactoryFunc));
    }

    void setConnectorFactoryFunc(ConnectorFactory::FactoryFunc newFactoryFunc)
    {
        auto oldFunc = ConnectorFactory::setFactoryFunc(std::move(newFactoryFunc));
        if (!m_oldFactoryFunc)
            m_oldFactoryFunc = oldFunc;
    }

    const hpm::MediatorFunctionalTest& mediator() const
    {
        return m_mediator;
    }

    hpm::MediatorFunctionalTest& mediator()
    {
        return m_mediator;
    }

protected:
    struct ConnectResult
    {
        SystemError::ErrorCode errorCode;
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection;
        std::chrono::milliseconds executionTime;
    };

    ConnectResult doSimpleConnectTest(
        std::chrono::milliseconds connectTimeout,
        MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
        boost::optional<SocketAddress> mediatorAddressForConnector = boost::none)
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

    ConnectResult doSimpleConnectTest(
        std::chrono::milliseconds connectTimeout,
        MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
        const nx::hpm::AbstractCloudDataProvider::System& system,
        const std::unique_ptr<MediaServerEmulator>& server,
        boost::optional<SocketAddress> mediatorAddressForConnector = boost::none)
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

private:
    boost::optional<ConnectorFactory::FactoryFunc> m_oldFactoryFunc;
    hpm::MediatorFunctionalTest m_mediator;

    void doSimpleConnectTest(
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

        std::promise<ConnectResult> connectedPromise;
        udp::TunnelConnector connector(
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
                : connectTimeout*2));
        *connectResult = connectedFuture.get();
        connectResult->executionTime = 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t1);

        connector.pleaseStopSync();
    }
};

TEST_F(TunnelConnector, general)
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

TEST_F(TunnelConnector, noSynAck)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds(5),
        MediaServerEmulator::ActionToTake::ignoreSyn);

    ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(TunnelConnector, badSynAck)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto connectResult = doSimpleConnectTest(
        std::chrono::seconds::zero(),   //no timeout
        MediaServerEmulator::ActionToTake::sendBadSynAck);

    ASSERT_EQ(SystemError::connectionReset, connectResult.errorCode);
    ASSERT_EQ(nullptr, connectResult.connection);
}

TEST_F(TunnelConnector, timeout)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const std::chrono::milliseconds connectTimeout(1000 + (rand() % 3000));
    //const std::chrono::milliseconds connectTimeout(5 + (rand() % 10));
    //const std::chrono::milliseconds connectTimeout(150 + (rand() % 150));

    for (int i = 0; i < 2; ++i)
    {
        boost::optional<SocketAddress> mediatorAddressForConnector;
        if ((i & 1) == 0)
        {
            //timing out udt connection...
        }
        else
        {
            //timing out mediator response by providing incorrect mediator address to connector
            mediatorAddressForConnector =
                SocketAddress(HostAddress::localhost, 10345);
        }

        const auto connectResult = doSimpleConnectTest(
            connectTimeout,
            MediaServerEmulator::ActionToTake::ignoreIndication,
            mediatorAddressForConnector);

        ASSERT_EQ(SystemError::timedOut, connectResult.errorCode);
        ASSERT_EQ(nullptr, connectResult.connection);
        ASSERT_TRUE(
            connectResult.executionTime > connectTimeout*0.8 &&
            connectResult.executionTime < connectTimeout*1.2);
    }
}

TEST_F(TunnelConnector, target_host_not_found)
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

TEST_F(TunnelConnector, cancellation)
{
    const std::chrono::seconds totalTestTime(10);

    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    const auto system = mediator().addRandomSystem();
    const auto server = mediator().addRandomServer(system);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server->listen());

    const auto t1 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t1 < totalTestTime)
    {
        udp::TunnelConnector connector(
            SocketAddress((server->serverId() + "." + system.id).constData()));

        connector.connect(
            std::chrono::milliseconds::zero(),
            [](
                SystemError::ErrorCode /*errorCode*/,
                std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)
            {
            });
        
        //implying random delay
        std::this_thread::sleep_for(std::chrono::microseconds(rand() & 0xffff) * 10);

        connector.pleaseStopSync();
    }
}

}   //namespace test
}   //namespace udp
}   //namespace cloud
}   //namespace network
}   //namespace nx
