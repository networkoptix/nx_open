#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/std/future.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_options.h>

#include "common_server_socket_ut.h"
#include "stream_socket_acceptance_tests.h"

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

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_createStreamSocketFunc;
    boost::optional<SocketFactory::CreateStreamServerSocketFuncType> m_createStreamServerSocketFunc;
};

namespace {

void onAcceptedConnection(
    AbstractStreamServerSocket* serverSocket,
    SystemError::ErrorCode /*errCode*/,
    std::unique_ptr<AbstractStreamSocket> sock)
{
    sock.reset();

    using namespace std::placeholders;
    serverSocket->acceptAsync(std::bind(onAcceptedConnection, serverSocket, _1, _2));
}

} // namespace

TEST_F(SocketUdt, cancelConnect)
{
    UdtStreamServerSocket serverSocket(AF_INET);
    ASSERT_TRUE(serverSocket.setNonBlockingMode(true));
    ASSERT_TRUE(serverSocket.bind(SocketAddress(HostAddress::localhost, 0)));
    serverSocket.listen();
    using namespace std::placeholders;
    serverSocket.acceptAsync(std::bind(onAcceptedConnection, &serverSocket, _1, _2));

    for (int i = 0; i < 100; ++i)
    {
        UdtStreamSocket sock(AF_INET);
        std::atomic<bool> handlerCalled(false);
        ASSERT_TRUE(sock.setNonBlockingMode(true));
        sock.connectAsync(
            serverSocket.getLocalAddress(),
            [&handlerCalled](SystemError::ErrorCode /*errorCode*/){
                handlerCalled = true;
            });
        sock.pleaseStopSync();

        std::this_thread::sleep_for(std::chrono::milliseconds(nx::utils::random::number(0, 50)));
    }

    serverSocket.pleaseStopSync();
}

#if 0
TEST(SocketUdt_UdtPollSet, general)
{
    UdtPollSet pollset;
    ASSERT_TRUE(pollset.isValid());

    UdtStreamSocket sock;

    ASSERT_TRUE(pollset.add(&sock, aio::etRead, (void*)1));
    ASSERT_TRUE(pollset.add(&sock, aio::etWrite, (void*)2));

    pollset.remove(&sock, aio::etRead);
    pollset.remove(&sock, aio::etWrite);
    ASSERT_TRUE(pollset.add(&sock, aio::etRead));
    pollset.remove(&sock, aio::etRead);

    auto result = pollset.poll(100);
    ASSERT_EQ(0, result);
}
#endif

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST_F, SocketUdt,
    [](){ return std::make_unique<UdtStreamServerSocket>(AF_INET); },
    [](){ return std::make_unique<UdtStreamSocket>(AF_INET); })

static std::unique_ptr<UdtStreamSocket> createRendezvousUdtSocket(
    std::chrono::milliseconds connectTimeout)
{
    auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
    EXPECT_TRUE(socket->setRendezvous(true));
    EXPECT_TRUE(socket->setSendTimeout(connectTimeout.count()));
    EXPECT_TRUE(socket->setNonBlockingMode(true));
    EXPECT_TRUE(socket->bind(SocketAddress(HostAddress::localhost, 0)));
    return socket;
}

TEST_F(SocketUdt, rendezvousConnect)
{
    const std::chrono::seconds kConnectTimeout(2);
    const size_t kBytesToSendThroughConnection(128 * 1024);
    const int kMaxSimultaneousConnections(25);
    const std::chrono::seconds kTestDuration(3);

    //creating two sockets, performing randezvous connect
    const auto connectorSocket = createRendezvousUdtSocket(kConnectTimeout);
    const auto acceptorSocket = createRendezvousUdtSocket(kConnectTimeout);

    auto socketStoppedGuard = makeScopeGuard(
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
    static const std::chrono::seconds kConnectTimeout;
    static const std::chrono::seconds kConnectDelay;
    static const size_t kBytesToEcho;
    static const int kMaxSimultaneousConnections;
    static const std::chrono::seconds kTestDuration;

    virtual void SetUp() override
    {
        m_serverSocket = createRendezvousUdtSocket(kConnectTimeout);
        m_clientSocket = createRendezvousUdtSocket(kConnectTimeout);

        setUdtSocketFunctions();
    }

    void startConnectingServerSocket()
    {
        m_serverSocket->connectAsync(
            m_clientSocket->getLocalAddress(),
            [this](
                SystemError::ErrorCode code)
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

private:
    std::unique_ptr<UdtStreamSocket> m_serverSocket;
    std::unique_ptr<UdtStreamSocket> m_clientSocket;
    std::unique_ptr<RandomDataTcpServer> m_server;
    std::unique_ptr<ConnectionsGenerator> m_generator;
    nx::utils::promise<SystemError::ErrorCode> m_serverSocketConnected;
    nx::utils::promise<SystemError::ErrorCode> m_clientSocketConnected;
};

const std::chrono::seconds SocketUdtRendezvous::kConnectTimeout = std::chrono::seconds::zero();
const std::chrono::seconds SocketUdtRendezvous::kConnectDelay(1);
const size_t SocketUdtRendezvous::kBytesToEcho(128 * 1024);
const int SocketUdtRendezvous::kMaxSimultaneousConnections(25);
const std::chrono::seconds SocketUdtRendezvous::kTestDuration(3);

TEST_F(SocketUdtRendezvous, connectWithDelay)
{
    startConnectingServerSocket();
    std::this_thread::sleep_for(kConnectDelay);
    startConnectingClientSocket();

    assertClientSocketHasConnected();
    assertServerSocketHasConnected();

    startConnectionGenerator();
    waitUntilSomeActivityHasBeenPerformedByGenerator();
}

TEST_F(SocketUdt, acceptingFirstConnection)
{
    const int loopLength = 71;

    for (int i = 0; i < loopLength; ++i)
    {
        UdtStreamServerSocket serverSocket(AF_INET);
        auto serverSocketGuard = makeScopeGuard(
            [&serverSocket]() { serverSocket.pleaseStopSync(); });

        ASSERT_TRUE(serverSocket.bind(SocketAddress(HostAddress::localhost, 0)));
        const auto serverAddress = serverSocket.getLocalAddress();
        ASSERT_TRUE(serverSocket.listen());
        ASSERT_TRUE(serverSocket.setNonBlockingMode(true));

        nx::utils::promise<
            std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
        > socketAcceptedPromise;
        serverSocket.acceptAsync(
            [&socketAcceptedPromise](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
               socketAcceptedPromise.set_value(
                   std::make_pair(errorCode, std::move(socket)));
            });

        UdtStreamSocket clientSock(AF_INET);
        ASSERT_TRUE(clientSock.connect(serverAddress, nx::network::kNoTimeout))
            << serverAddress.toStdString() <<", "
            << SystemError::getLastOSErrorText().toStdString();

        const auto result = socketAcceptedPromise.get_future().get();
        ASSERT_EQ(SystemError::noError, result.first);
    }
}

class SocketUdtNew:
    public SocketUdt
{
public:
    SocketUdtNew():
        m_serverSocket(AF_INET)
    {
    }

protected:
    std::unique_ptr<UdtStreamSocket> m_clientSock;
    UdtStreamServerSocket m_serverSocket;

    void startSyncUdtSocketServer()
    {
        m_udtSocketServerThread =
            nx::utils::thread(std::bind(&SocketUdtNew::udtSocketServerMain, this));
        m_udtSocketServerStarted.get_future().wait();
    }

    void listenOnServerSocket()
    {
        ASSERT_TRUE(m_serverSocket.bind(SocketAddress(HostAddress::localhost, 0)));
        m_serverAddress = m_serverSocket.getLocalAddress();
        ASSERT_TRUE(m_serverSocket.listen());
    }

    void connectToUdtServer()
    {
        m_clientSock = std::make_unique<UdtStreamSocket>(AF_INET);
        ASSERT_TRUE(m_clientSock->connect(m_serverAddress, nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();
    }

    void closeConnection()
    {
        m_clientSock.reset();
    }

    void assertAcceptedConnectionReceivedFin()
    {
        m_udtSocketServerThread.join();
    }

private:
    nx::utils::thread m_udtSocketServerThread;
    nx::utils::promise<void> m_udtSocketServerStarted;
    SocketAddress m_serverAddress;

    void udtSocketServerMain()
    {
        listenOnServerSocket();

        m_udtSocketServerStarted.set_value();

        std::unique_ptr<AbstractStreamSocket> acceptedSocket(m_serverSocket.accept());
        ASSERT_NE(nullptr, acceptedSocket);

        for (;;)
        {
            char testBuf[128];
            const int bytesRead = acceptedSocket->recv(testBuf, sizeof(testBuf));
            ASSERT_GE(bytesRead, 0);
            if (bytesRead == 0)
                break;
        }
    }
};

TEST_F(SocketUdtNew, simpleTest)
{
    listenOnServerSocket();

    for (int i = 0; i < 17; ++i)
    {
        connectToUdtServer();

        std::unique_ptr<AbstractStreamSocket> acceptedSocket(m_serverSocket.accept());
        ASSERT_NE(nullptr, acceptedSocket);

        std::array<char, 128> testBuf;
        ASSERT_EQ((int)testBuf.size(), m_clientSock->send(testBuf.data(), testBuf.size()));

        std::array<char, testBuf.size()> readBuf;
        ASSERT_EQ((int)readBuf.size(), acceptedSocket->recv(readBuf.data(), readBuf.size(), 0));

        closeConnection();
    }
}

TEST_F(SocketUdtNew, connectionProperlyClosed)
{
    startSyncUdtSocketServer();
    connectToUdtServer();

    closeConnection();
    assertAcceptedConnectionReceivedFin();
}

TEST_F(SocketUdt, allDataReadAfterFin)
{
    const int testMessageLength = 16;
    const std::chrono::milliseconds connectTimeout(3000);
    const std::chrono::milliseconds recvTimeout(3000);

    for (int i = 0; i < 2; ++i)
    {
        UdtStreamServerSocket server(AF_INET);
        auto serverGuard = makeScopeGuard([&server]() { server.pleaseStopSync(); });

        ASSERT_TRUE(server.setNonBlockingMode(true));
        ASSERT_TRUE(server.bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(server.listen());

        nx::utils::promise<
            std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
        > connectionAcceptedPromise;

        server.acceptAsync(
            [&connectionAcceptedPromise](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> sock)
            {
                connectionAcceptedPromise.set_value(
                    std::make_pair(errorCode, std::move(sock)));
            });

        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto clientSock = std::make_shared<UdtStreamSocket>(AF_INET);
        ASSERT_TRUE(clientSock->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_NE(server.getLocalAddress(), clientSock->getLocalAddress());

        ASSERT_TRUE(clientSock->connect(server.getLocalAddress(), nx::network::kNoTimeout));

        auto connectionAcceptedFuture = connectionAcceptedPromise.get_future();
        ASSERT_EQ(
            std::future_status::ready,
            connectionAcceptedFuture.wait_for(connectTimeout));
        auto acceptResult = connectionAcceptedFuture.get();
        ASSERT_EQ(SystemError::noError, acceptResult.first);

        const QByteArray testMessage = nx::utils::generateRandomName(testMessageLength);
        if (i == 0)
        {
            ASSERT_EQ(
                testMessage.size(),
                clientSock->send(testMessage.constData(), testMessage.size()));
            clientSock.reset();
        }
        else
        {
            ASSERT_TRUE(clientSock->setNonBlockingMode(true));
            auto clientSockPtr = clientSock.get();
            clientSockPtr->sendAsync(
                testMessage,
                [clientSock = std::move(clientSock), msgSize = testMessage.size()](
                    SystemError::ErrorCode errorCode, size_t bytesSent) mutable
                {
                    ASSERT_EQ(SystemError::noError, errorCode);
                    ASSERT_EQ((size_t)msgSize, bytesSent);
                    clientSock.reset();
                });
        }

        //reading out accepted socket
        auto acceptedSocket = std::move(acceptResult.second);

        ASSERT_TRUE(acceptedSocket->setRecvTimeout(recvTimeout.count()))
            << SystemError::getLastOSErrorText().toStdString();

        const auto recvBuffer = readNBytes(acceptedSocket.get(), testMessage.size());
        ASSERT_EQ(testMessage, recvBuffer)
            << SystemError::getLastOSErrorText().toStdString();
    }
}

class UdtSocketPerformance:
    public ::testing::Test
{
protected:
    const uint64_t kBufferSize = 4 * 1024;
    const uint64_t kTransferSize = 100 * 1024 * 1024;
    const uint64_t kConcurentCount = 10;

    static uint64_t selectTransferSize()
    {
        return utils::TestOptions::applyLoadMode((uint64_t) 1024) * 1024 * 1024;
    }

    UdtSocketPerformance():
        kTransferSize(selectTransferSize())
    {
    }

    void SetUp() override
    {
        server = std::make_unique<UdtStreamServerSocket>(AF_INET);
        ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(server->listen(10));

        address = server->getLocalAddress();
        NX_LOGX(lm("Server address=%1, transferSize=%2b")
            .args(address, nx::utils::bytesToString(kTransferSize)), cl_logINFO);

        ASSERT_TRUE(server->setRecvTimeout(100));
        ASSERT_EQ(nullptr, server->accept());
        ASSERT_TRUE(server->setRecvTimeout(0));
    }

    std::unique_ptr<UdtStreamSocket> connect() const
    {
        auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
        socketConfig(socket.get());
        EXPECT_TRUE(socket->connect(address, nx::network::kNoTimeout));
        return socket;
    }

    std::unique_ptr<AbstractStreamSocket> accept() const
    {
        std::unique_ptr<AbstractStreamSocket> socket(server->accept());
        EXPECT_NE(nullptr, socket);
        socketConfig(socket.get());
        return socket;
    }

    std::unique_ptr<UDPSocket> udpSocket() const
    {
        auto socket = std::make_unique<UDPSocket>(AF_INET);
        EXPECT_TRUE(socket->bind(SocketAddress::anyPrivateAddress));
        EXPECT_TRUE(socket->setSendBufferSize(4 * 1024 * 1024));
        EXPECT_TRUE(socket->setRecvBufferSize(4 * 1024 * 1024));
        return socket;
    }

    void socketConfig(AbstractSocket* /*socket*/) const
    {
        //EXPECT_TRUE(socket->setSendBufferSize(100 * 1024 * 1024));
        //EXPECT_TRUE(socket->setRecvBufferSize(100 * 1024 * 1024));
    }

    void sendSync(AbstractStreamSocket* socket) const
    {
        Buffer buffer((int) kBufferSize, 'X');
        int send = 0;
        uint64_t transferSize = 0;
        while (transferSize < kTransferSize)
        {
            send = socket->send(buffer.data(), buffer.size());
            if (send <= 0)
                break;

            transferSize += (uint64_t) send;
        }
    }

    void recvSync(AbstractStreamSocket* socket)
    {
        const auto startTime = std::chrono::steady_clock::now();
        Buffer buffer((int) kBufferSize, Qt::Uninitialized);

        int recv = 0;
        uint64_t transferSize = 0;
        uint64_t transferCount = 0;
        while (transferSize < kTransferSize)
        {
            recv = socket->recv(buffer.data(), buffer.size());
            if (recv <= 0)
                break;

            transferSize += (uint64_t) recv;
            transferCount += 1;
        }

        recvLog(recv, std::chrono::steady_clock::now() - startTime, transferSize, transferCount);
    }

    void recvLog(int lastRecv, std::chrono::steady_clock::duration duration,
        uint64_t transferSize, uint64_t transferCount) const
    {
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        NX_LOGX(lm("Resieve ended (%1): %2")
            .args(lastRecv, SystemError::getLastOSErrorText()), cl_logINFO);

        const auto bytesPerS = double(transferSize) * 1000 / durationMs.count();
        NX_LOGX(lm("Resieved size=%1b, count=%2, average=%3, duration=%4, speed=%5bps")
            .args(nx::utils::bytesToString(transferSize), transferCount,
                nx::utils::bytesToString(transferSize / transferCount),
                durationMs, nx::utils::bytesToString((uint64_t) bytesPerS)), cl_logINFO);

        // TODO: some assertions about speed?
    }

    std::unique_ptr<UdtStreamServerSocket> server;
    SocketAddress address;
};

TEST_F(UdtSocketPerformance, DISABLED_SimplexSync)
{
    nx::utils::thread acceptThread(
        [&]()
        {
            const auto client = accept();
            recvSync(client.get());
            sendSync(client.get());
        });

    nx::utils::thread clientThread(
        [&]()
        {
            const auto client = connect();
            sendSync(client.get());
            recvSync(client.get());
        });

    acceptThread.join();
    clientThread.join();
}

TEST_F(UdtSocketPerformance, DISABLED_DuplexSync)
{
    nx::utils::thread acceptThread(
        [&]()
        {
            const auto client = accept();
            nx::utils::thread t([&](){ recvSync(client.get()); });
            sendSync(client.get());
            t.join();
        });

    nx::utils::thread clientThread(
        [&]()
        {
            const auto client = connect();
            nx::utils::thread t([&](){ sendSync(client.get()); });
            recvSync(client.get());
            t.join();
        });

    acceptThread.join();
    clientThread.join();
}

INSTANTIATE_TYPED_TEST_CASE_P(UdtStreamServerSocket, ServerSocketTest, UdtStreamServerSocket);

struct UdtSocketTypeSet
{
    using ClientSocket = UdtStreamSocket;
    using ServerSocket = UdtStreamServerSocket;
};

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
