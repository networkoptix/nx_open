#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>
#include <nx/utils/std/future.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace network {
namespace test {

class SocketUdt:
    public ::testing::Test
{
public:
    ~SocketUdt()
    {
        if (m_createStreamSocketFunc)
            SocketFactory::setCreateStreamSocketFunc(
                std::move(*m_createStreamSocketFunc));
        if (m_createStreamServerSocketFunc)
            SocketFactory::setCreateStreamServerSocketFunc(
                std::move(*m_createStreamServerSocketFunc));
    }

    void setCreateStreamSocketFunc(
        SocketFactory::CreateStreamSocketFuncType newFactoryFunc)
    {
        auto oldFunc =
            SocketFactory::setCreateStreamSocketFunc(std::move(newFactoryFunc));
        if (!m_createStreamSocketFunc)
            m_createStreamSocketFunc = oldFunc;
    }

    void setCreateStreamServerSocketFunc(
        SocketFactory::CreateStreamServerSocketFuncType newFactoryFunc)
    {
        auto oldFunc =
            SocketFactory::setCreateStreamServerSocketFunc(std::move(newFactoryFunc));
        if (!m_createStreamServerSocketFunc)
            m_createStreamServerSocketFunc = oldFunc;
    }

    void setUdtSocketFunctions()
    {
        setCreateStreamSocketFunc(
            [](bool /*sslRequired*/, NatTraversalSupport, boost::optional<int> /*ipVersion*/)
                -> std::unique_ptr<AbstractStreamSocket>
            {
                return std::make_unique<UdtStreamSocket>(AF_INET);
            });

        setCreateStreamServerSocketFunc(
            [](bool /*sslRequired*/, NatTraversalSupport, boost::optional<int> /*ipVersion*/)
                -> std::unique_ptr<AbstractStreamServerSocket>
            {
                return std::make_unique<UdtStreamServerSocket>(AF_INET);
            });
    }

protected:
    void startSilentUdpServer()
    {
        m_udpSocket = std::make_unique<UDPSocket>(AF_INET);
        ASSERT_TRUE(m_udpSocket->bind(SocketAddress::anyPrivateAddress));

        m_serverEndpoint = m_udpSocket->getLocalAddress();
    }

    void whenConnectWithTimeout(std::chrono::milliseconds timeout)
    {
        initializeClientSocketIfNeeded();

        m_lastConnectResult = m_clientSocket->connect(m_serverEndpoint, timeout)
            ? SystemError::noError
            : SystemError::getLastOSErrorCode();
    }

    void thenConnectFailedWithError(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, *m_lastConnectResult);
    }

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_createStreamSocketFunc;
    boost::optional<SocketFactory::CreateStreamServerSocketFuncType> m_createStreamServerSocketFunc;
    std::unique_ptr<UDPSocket> m_udpSocket;
    std::unique_ptr<UdtStreamSocket> m_clientSocket;
    SocketAddress m_serverEndpoint;
    std::optional<SystemError::ErrorCode> m_lastConnectResult;

    void initializeClientSocketIfNeeded()
    {
        m_clientSocket = std::make_unique<UdtStreamSocket>(AF_INET);
    }
};

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST_F, SocketUdt,
    [](){ return std::make_unique<UdtStreamServerSocket>(AF_INET); },
    [](){ return std::make_unique<UdtStreamSocket>(AF_INET); })

//-------------------------------------------------------------------------------------------------

TEST_F(SocketUdt, connect_timeout)
{
    startSilentUdpServer();

    whenConnectWithTimeout(std::chrono::milliseconds(1));
    thenConnectFailedWithError(SystemError::timedOut);
}

//-------------------------------------------------------------------------------------------------

class SocketUdtRendezvous:
    public SocketUdt
{
public:
    ~SocketUdtRendezvous()
    {
        if (m_serverSocket)
            m_serverSocket->pleaseStopSync();
        if (m_clientSocket)
            m_clientSocket->pleaseStopSync();
        if (m_generator)
            m_generator->pleaseStopSync();
        if (m_server)
            m_server->pleaseStopSync();
    }

protected:
    static const std::chrono::seconds kConnectDelay;
    static const size_t kBytesToEcho;
    static const int kMaxSimultaneousConnections;
    static const std::chrono::seconds kTestDuration;

    virtual void SetUp() override
    {
        m_serverSocket = createRendezvousUdtSocket();
        m_clientSocket = createRendezvousUdtSocket();

        setUdtSocketFunctions();
    }

    void setConnectTimeout(std::chrono::milliseconds timeout)
    {
        m_connectTimeout = timeout;
    }

    void startConnectingServerSocket()
    {
        m_serverSocket->connectAsync(
            m_clientSocket->getLocalAddress(),
            [this](SystemError::ErrorCode code)
            {
                m_clientSocketConnected.set_value(code);
            });
    }

    void assertServerSocketHasConnected()
    {
        const auto connectResultCode = m_clientSocketConnected.get_future().get();
        ASSERT_EQ(SystemError::noError, connectResultCode)
            << SystemError::toString(connectResultCode).toStdString();

        auto serverSocketReadBuffer = std::make_shared<Buffer>();
        serverSocketReadBuffer->reserve(100);
        m_serverSocket->readSomeAsync(
            serverSocketReadBuffer.get(),
            [serverSocketReadBuffer](SystemError::ErrorCode code, size_t size)
            {
                // No data is supposed to be sent using through this connection.
                EXPECT_TRUE(code != SystemError::noError || size == 0);
            });
    }

    void assertServerSocketConnectCompleted()
    {
        m_clientSocketConnected.get_future().wait();
    }

    void startConnectingClientSocket()
    {
        m_clientSocket->connectAsync(
            m_serverSocket->getLocalAddress(),
            [this](SystemError::ErrorCode code)
            {
                m_serverSocketConnected.set_value(code);
            });
    }

    void assertClientSocketHasConnected()
    {
        const auto connectResultCode = m_serverSocketConnected.get_future().get();
        ASSERT_EQ(SystemError::noError, connectResultCode)
            << SystemError::toString(connectResultCode).toStdString();

        auto buffer = std::make_shared<Buffer>();
        buffer->reserve(100);
        m_clientSocket->readSomeAsync(
            buffer.get(),
            [buffer](SystemError::ErrorCode code, size_t size)
            {
                // no data is supposed to send using this socket
                ASSERT_TRUE(code != SystemError::noError || size == 0);
            });
    }

    void closeClientSocket()
    {
        m_clientSocket->pleaseStopSync();
        m_clientSocket.reset();
    }

    void closeServerSocket()
    {
        m_serverSocket->pleaseStopSync();
        m_serverSocket.reset();
    }

    void startConnectionGenerator()
    {
        m_server.reset(new RandomDataTcpServer(
            TestTrafficLimitType::none, 0, TestTransmissionMode::pong));
        m_server->setLocalAddress(m_serverSocket->getLocalAddress());
        ASSERT_TRUE(m_server->start());

        SocketAddress serverAddress(
            HostAddress::localhost, m_server->addressBeingListened().port);
        m_generator.reset(new ConnectionsGenerator(
            serverAddress, kMaxSimultaneousConnections,
            TestTrafficLimitType::incoming, kBytesToEcho,
            ConnectionsGenerator::kInfiniteConnectionCount,
            TestTransmissionMode::ping));

        m_generator->setLocalAddress(m_clientSocket->getLocalAddress());
        m_generator->start();
    }

    void waitUntilSomeActivityHasBeenPerformedByGenerator()
    {
        const int kTotalConnectionsEstablished = 7;

        for (;;)
        {
            if (m_generator->totalConnectionsEstablished() > kTotalConnectionsEstablished &&
                m_generator->totalBytesSent() > 0U &&
                m_generator->totalBytesReceived() > 0U)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::unique_ptr<UdtStreamSocket> createRendezvousUdtSocket()
    {
        auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
        EXPECT_TRUE(socket->setRendezvous(true));
        if (m_connectTimeout)
        {
            EXPECT_TRUE(socket->setSendTimeout(m_connectTimeout->count()));
        }
        EXPECT_TRUE(socket->setNonBlockingMode(true));
        EXPECT_TRUE(socket->bind(SocketAddress(HostAddress::localhost, 0)));
        return socket;
    }

private:
    std::unique_ptr<UdtStreamSocket> m_serverSocket;
    std::unique_ptr<UdtStreamSocket> m_clientSocket;
    std::unique_ptr<RandomDataTcpServer> m_server;
    std::unique_ptr<ConnectionsGenerator> m_generator;
    nx::utils::promise<SystemError::ErrorCode> m_serverSocketConnected;
    nx::utils::promise<SystemError::ErrorCode> m_clientSocketConnected;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
};

const std::chrono::seconds SocketUdtRendezvous::kConnectDelay(1);
const size_t SocketUdtRendezvous::kBytesToEcho(128 * 1024);
const int SocketUdtRendezvous::kMaxSimultaneousConnections(25);
const std::chrono::seconds SocketUdtRendezvous::kTestDuration(3);

TEST_F(SocketUdtRendezvous, reusing_rendezvous_connection_stress_test)
{
    const std::chrono::seconds kConnectTimeout(2);
    const size_t kBytesToSendThroughConnection(128 * 1024);
    const int kMaxSimultaneousConnections(25);
    const std::chrono::seconds kTestDuration(3);

    setConnectTimeout(kConnectTimeout);

    //creating two sockets, performing randezvous connect
    const auto connectorSocket = createRendezvousUdtSocket();
    const auto acceptorSocket = createRendezvousUdtSocket();

    auto socketStoppedGuard = nx::utils::makeScopeGuard(
        [&connectorSocket, &acceptorSocket]
        {
            //cleaning up
            connectorSocket->pleaseStopSync();
            acceptorSocket->pleaseStopSync();
        });

    nx::utils::promise<SystemError::ErrorCode> connectorConnectedPromise;
    connectorSocket->connectAsync(
        acceptorSocket->getLocalAddress(),
        [&connectorConnectedPromise](
            SystemError::ErrorCode errorCode)
        {
            connectorConnectedPromise.set_value(errorCode);
        });

    nx::utils::promise<SystemError::ErrorCode> acceptorConnectedPromise;
    acceptorSocket->connectAsync(
        connectorSocket->getLocalAddress(),
        [&acceptorConnectedPromise](
            SystemError::ErrorCode errorCode)
        {
            acceptorConnectedPromise.set_value(errorCode);
        });

    const auto connectorResultCode = connectorConnectedPromise.get_future().get();
    ASSERT_EQ(SystemError::noError, connectorResultCode)
        << SystemError::toString(connectorResultCode).toStdString();
    const auto acceptorResultCode = acceptorConnectedPromise.get_future().get();
    ASSERT_EQ(SystemError::noError, acceptorResultCode)
        << SystemError::toString(acceptorResultCode).toStdString();

    //after successfull connect starting listener on one side and connector on the other one
    setUdtSocketFunctions();

    RandomDataTcpServer server(
        TestTrafficLimitType::outgoing,
        kBytesToSendThroughConnection,
        TestTransmissionMode::spam);
    server.setLocalAddress(acceptorSocket->getLocalAddress());
    ASSERT_TRUE(server.start());

    ConnectionsGenerator connectionsGenerator(
        SocketAddress(HostAddress::localhost, server.addressBeingListened().port),
        kMaxSimultaneousConnections,
        TestTrafficLimitType::outgoing,
        kBytesToSendThroughConnection,
        ConnectionsGenerator::kInfiniteConnectionCount,
        TestTransmissionMode::spam);
    connectionsGenerator.setLocalAddress(connectorSocket->getLocalAddress());
    connectionsGenerator.start();

    std::this_thread::sleep_for(kTestDuration);

    connectionsGenerator.pleaseStopSync();
    server.pleaseStopSync();

    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0U);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0U);
    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0U);
}

TEST_F(SocketUdtRendezvous, connect_works)
{
    startConnectingServerSocket();
    startConnectingClientSocket();

    assertClientSocketHasConnected();
    assertServerSocketHasConnected();
}

TEST_F(SocketUdtRendezvous, socket_is_closed_during_connect)
{
    setConnectTimeout(std::chrono::milliseconds(1));

    startConnectingServerSocket();
    startConnectingClientSocket();

    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));

    closeClientSocket();
    closeServerSocket();
}

TEST_F(SocketUdtRendezvous, connect_works_with_delay_between_server_and_client)
{
    startConnectingServerSocket();
    std::this_thread::sleep_for(kConnectDelay);
    startConnectingClientSocket();

    assertClientSocketHasConnected();
    assertServerSocketHasConnected();

    startConnectionGenerator();
    waitUntilSomeActivityHasBeenPerformedByGenerator();
}

//-------------------------------------------------------------------------------------------------

struct UdtSocketTypeSet
{
    using ClientSocket = UdtStreamSocket;
    using ServerSocket = UdtStreamServerSocket;

    static const SocketAddress serverEndpointForConnectShutdown;
};

// Any endpoint that does not have UDT server socket on it is fine.
// TODO: #ak Allocate UDP socket and use that socket's port.
const SocketAddress UdtSocketTypeSet::serverEndpointForConnectShutdown =
    SocketAddress(HostAddress::localhost, 45431);

INSTANTIATE_TYPED_TEST_CASE_P(UdtSocketStream, StreamSocketAcceptance, UdtSocketTypeSet);

//-------------------------------------------------------------------------------------------------

#if 0
struct UdtStreamSocketFactory
{
    std::unique_ptr<nx::network::UdtStreamSocket> operator()() const
    {
        return std::make_unique<nx::network::UdtStreamSocket>(AF_INET);
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(UdtStreamSocket, SocketOptions, UdtStreamSocketFactory);
INSTANTIATE_TYPED_TEST_CASE_P(UdtStreamSocket, SocketOptionsDefaultValue, UdtStreamSocketFactory);
#endif

} // namespace test
} // namespace network
} // namespace nx
