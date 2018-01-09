#include <chrono>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>
#include <nx/network/test_support/test_outgoing_tunnel.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {

namespace test {

struct CloudStreamSocketTypeSet
{
    using ClientSocket = cloud::CloudStreamSocket;
    using ServerSocket = TCPServerSocket;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    CloudStreamSocket,
    StreamSocketAcceptance,
    CloudStreamSocketTypeSet);

} // namespace test

namespace cloud {
namespace test {

NX_NETWORK_CLIENT_SOCKET_TEST_CASE(
    TEST, CloudStreamSocketTcpByIp,
    []() { return std::make_unique<TCPServerSocket>(AF_INET); },
    []() { return std::make_unique<CloudStreamSocket>(AF_INET); });

TEST(CloudStreamSocketTcpByIp, TransferSyncSsl)
{
    network::test::socketTransferSync(
        [&]()
        {
            return std::make_unique<deprecated::SslServerSocket>(
                std::make_unique<TCPServerSocket>(AF_INET), false);
        },
        []()
        {
            return std::make_unique<deprecated::SslSocket>(
                std::make_unique<CloudStreamSocket>(AF_INET), false);
        });
}

class TestTcpServerSocket:
    public TCPServerSocket
{
public:
    TestTcpServerSocket(nx::network::HostAddress host):
        TCPServerSocket(AF_INET),
        m_host(std::move(host))
    {
    }

    ~TestTcpServerSocket()
    {
        if (m_socketAddress)
            SocketGlobals::addressResolver().removeFixedAddress(m_host, *m_socketAddress);
    }

    virtual bool bind(const nx::network::SocketAddress& localAddress) override
    {
        if (!TCPServerSocket::bind(localAddress))
            return false;

        m_socketAddress = TCPServerSocket::getLocalAddress();
        SocketGlobals::addressResolver().addFixedAddress(m_host, *m_socketAddress);
        return true;
    }

    virtual nx::network::SocketAddress getLocalAddress() const override
    {
        return m_socketAddress ? *m_socketAddress : nx::network::SocketAddress();
    }

private:
    const nx::network::HostAddress m_host;
    boost::optional<nx::network::SocketAddress> m_socketAddress;
};

struct CloudStreamSocketTcpByHost:
    public ::testing::Test
{
    const nx::network::HostAddress testHost;
    CloudStreamSocketTcpByHost(): testHost(id() + lit(".") + id()) {}
    static QString id() { return QnUuid::createUuid().toSimpleString(); }
};

NX_NETWORK_CLIENT_SOCKET_TEST_CASE_EX(
    TEST_F, CloudStreamSocketTcpByHost,
    [&]() { return std::make_unique<TestTcpServerSocket>(testHost); },
    [&]() { return std::make_unique<CloudStreamSocket>(AF_INET); },
    nx::network::SocketAddress(testHost));

TEST_F(CloudStreamSocketTcpByHost, TransferSyncSsl)
{
    network::test::socketTransferSync(
        [&]()
        {
            return std::make_unique<deprecated::SslServerSocket>(
                std::make_unique<TestTcpServerSocket>(testHost), false);
        },
        []()
        {
            return std::make_unique<deprecated::SslSocket>(
                std::make_unique<CloudStreamSocket>(AF_INET), false);
        },
        nx::network::SocketAddress(testHost));
}

//-------------------------------------------------------------------------------------------------

class CloudStreamSocketTest:
    public ::testing::Test
{
public:
    ~CloudStreamSocketTest()
    {
        if (m_oldCreateStreamSocketFunc)
            SocketFactory::setCreateStreamSocketFunc(std::move(*m_oldCreateStreamSocketFunc));
    }

    void setCreateStreamSocketFunc(
        SocketFactory::CreateStreamSocketFuncType newFactoryFunc)
    {
        auto oldFunc = SocketFactory::setCreateStreamSocketFunc(std::move(newFactoryFunc));
        if (!m_oldCreateStreamSocketFunc)
            m_oldCreateStreamSocketFunc = std::move(oldFunc);
    }

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_oldCreateStreamSocketFunc;
};

TEST_F(CloudStreamSocketTest, simple)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128*1024;
    const size_t repeatCount = 100;

    //starting local tcp server
    network::test::RandomDataTcpServer server(
        network::test::TestTrafficLimitType::outgoing,
        bytesToSendThroughConnection,
        network::test::TestTransmissionMode::spam);
    ASSERT_TRUE(server.start());
    auto serverGuard = makeScopeGuard([&server]() { server.pleaseStopSync(); });

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);
    auto tempHostNameGuard = makeScopeGuard(
        [&tempHostName, &serverAddress]()
        {
            nx::network::SocketGlobals::addressResolver().removeFixedAddress(
                tempHostName,
                serverAddress);
        });

    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(nx::network::SocketAddress(tempHostName), nx::network::kNoTimeout));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
        ASSERT_EQ(bytesToSendThroughConnection, (size_t)bytesRead);
    }

    // also try to connect just by system name
    {
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(nx::network::SocketAddress("bla"), nx::network::kNoTimeout));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
        ASSERT_EQ(bytesToSendThroughConnection, (size_t)bytesRead);
    }
}

TEST_F(CloudStreamSocketTest, multiple_connections_random_data)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t maxSimultaneousConnections = 1000;
    const std::chrono::seconds testDuration(3);

    setCreateStreamSocketFunc(
        []( bool /*sslRequired*/,
            nx::network::NatTraversalSupport /*natTraversalRequired*/,
            boost::optional<int> /*ipVersion*/) ->
                std::unique_ptr< nx::network::AbstractStreamSocket >
        {
            return std::make_unique<CloudStreamSocket>(AF_INET);
        });

    //starting local tcp server
    network::test::RandomDataTcpServer server(
        network::test::TestTrafficLimitType::outgoing,
        bytesToSendThroughConnection,
        network::test::TestTransmissionMode::spam);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    network::test::ConnectionsGenerator connectionsGenerator(
        nx::network::SocketAddress(tempHostName),
        maxSimultaneousConnections,
        network::test::TestTrafficLimitType::outgoing,
        bytesToSendThroughConnection,
        network::test::ConnectionsGenerator::kInfiniteConnectionCount,
        network::test::TestTransmissionMode::spam);
    connectionsGenerator.start();

    std::this_thread::sleep_for(testDuration);

    connectionsGenerator.pleaseStopSync();
    server.pleaseStopSync();

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

const auto createServerSocketFunc =
    []() -> std::unique_ptr<AbstractStreamServerSocket>
    {
        return SocketFactory::createStreamServerSocket();
    };
const auto createClientSocketFunc =
    []() -> std::unique_ptr<CloudStreamSocket>
    {
        return std::make_unique<CloudStreamSocket>(AF_INET);
    };

TEST_F(CloudStreamSocketTest, cancellation)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t repeatCount = 100;

    //starting local tcp server
    network::test::RandomDataTcpServer server(
        network::test::TestTrafficLimitType::outgoing,
        bytesToSendThroughConnection,
        network::test::TestTransmissionMode::spam);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    const auto scopedGuard = makeScopeGuard(
        [&]()
        {
            nx::network::SocketGlobals::addressResolver().removeFixedAddress(
                tempHostName,
                serverAddress);
            server.pleaseStopSync();
        });

    //cancelling connect
    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();
        cloudSocket.connectAsync(
            nx::network::SocketAddress(tempHostName),
            [](SystemError::ErrorCode /*code*/){});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    //cancelling read
    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(nx::network::SocketAddress(tempHostName), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();
        QByteArray data;
        data.reserve(bytesToSendThroughConnection);
        ASSERT_TRUE(cloudSocket.setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();
        cloudSocket.readSomeAsync(
            &data,
            [](SystemError::ErrorCode /*errorCode*/, size_t /*bytesRead*/) {});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    //cancelling send
    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(nx::network::SocketAddress(tempHostName), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        ASSERT_TRUE(cloudSocket.setNonBlockingMode(true))
            << SystemError::getLastOSErrorText().toStdString();
        cloudSocket.sendAsync(
            data,
            [](SystemError::ErrorCode /*errorCode*/, size_t /*bytesSent*/) {});
        cloudSocket.cancelIOSync(aio::etNone);
    }
}

//-------------------------------------------------------------------------------------------------

class CloudStreamSocketCancellation:
    public CloudStreamSocketTest
{
    using base_type = CloudStreamSocketTest;

public:
    enum class SocketState
    {
        init,
        connected,
        closed,
    };

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_tcpServer = std::make_unique<network::test::RandomDataTcpServer>(
            network::test::TestTrafficLimitType::none,
            0,
            network::test::TestTransmissionMode::spam);
        m_tcpServer->setLocalAddress(nx::network::SocketAddress(nx::network::HostAddress::localhost, 0));
        ASSERT_TRUE(m_tcpServer->start());
    }

    virtual void TearDown() override
    {
        base_type::TearDown();

        if (m_tcpServer)
            m_tcpServer->pleaseStopSync();
    }

    nx::network::SocketAddress serverAddress() const
    {
        return m_tcpServer->addressBeingListened();
    }

private:
    std::unique_ptr<network::test::RandomDataTcpServer> m_tcpServer;
};

TEST_F(CloudStreamSocketCancellation, syncModeCancellation)
{
    constexpr const std::chrono::milliseconds kTestDuration =
        std::chrono::milliseconds(2000);
    constexpr const int kTestRuns = 10;

    //launching some server

    for (int i = 0; i < kTestRuns; ++i)
    {
        auto socket = std::make_unique<CloudStreamSocket>(SocketFactory::tcpServerIpVersion());
        std::atomic<SocketState> socketState(SocketState::init);

        nx::utils::thread sendThread(
            [&socket, targetAddress = serverAddress(), &socketState]
            {
                char testBuffer[16*1024];

                if (!socket->isConnected())
                {
                    ASSERT_TRUE(socket->connect(targetAddress, nx::network::kNoTimeout));
                    socketState = SocketState::connected;
                }

                while (!socket->isClosed() && socket->isConnected())
                {
                    int bytesSent = socket->send(testBuffer, sizeof(testBuffer));
                    if (bytesSent == -1)
                        continue;
                }
            });

        //read thread
        nx::utils::thread recvThread(
            [&socket, &socketState]
            {
                char readBuffer[16 * 1024];
                for (;;)
                {
                    if (socketState == SocketState::init)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }

                    if (!socket->isConnected() || socket->isClosed())
                        break;

                    int bytesReceived = socket->recv(readBuffer, sizeof(readBuffer));
                    if (bytesReceived == -1)
                        continue;
                }
            });

        std::this_thread::sleep_for(kTestDuration);

        //cancelling
        if (i & 1)
            socket->close();
        else
            socket->shutdown();

        recvThread.join();
        sendThread.join();

        socket.reset();
    }
}

namespace {

class TestSocket:
    public TCPSocket
{
public:
    TestSocket(std::tuple<nx::network::SocketAddress, nx::utils::promise<void>*, nx::utils::promise<bool>*> args):
        TCPSocket(SocketFactory::tcpClientIpVersion())
    {
        m_socketInRecv = std::get<1>(args);
        m_continueReadingSocket = std::get<2>(args);
    }

    ~TestSocket()
    {
    }

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override
    {
        nx::utils::ObjectDestructionFlag::Watcher objectDestructionFlag(&m_destructionFlag);
        m_socketInRecv->set_value();
        m_continueReadingSocket->get_future().wait();

        const auto result = TCPSocket::recv(buffer, bufferLen, flags);

        assertIfThisHasBeenDestroyed(objectDestructionFlag);

        return result;
    }

private:
    nx::utils::promise<void>* m_socketInRecv;
    nx::utils::promise<bool>* m_continueReadingSocket;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    void assertIfThisHasBeenDestroyed(
        const nx::utils::ObjectDestructionFlag::Watcher& objectDestructionFlag)
    {
        ASSERT_FALSE(objectDestructionFlag.objectDestroyed());
    }
};

using TestOutgoingTunnelConnection = test::OutgoingTunnelConnection<
    TestSocket,
    nx::network::SocketAddress,
    nx::utils::promise<void>*,
    nx::utils::promise<bool>*>;

using TestCrossNatConnector = test::CrossNatConnector<
    TestOutgoingTunnelConnection,
    nx::network::SocketAddress,
    nx::utils::promise<void>*,
    nx::utils::promise<bool>*>;

} // namespace

//-------------------------------------------------------------------------------------------------

class CloudStreamSocketShutdown:
    public CloudStreamSocketCancellation
{
    using base_type = CloudStreamSocketCancellation;

public:
    CloudStreamSocketShutdown():
        m_terminated(false)
    {
        using namespace std::placeholders;

        m_oldFactory = CrossNatConnectorFactory::instance().setCustomFunc(
            std::bind(&CloudStreamSocketShutdown::connectorFactoryFunc, this, _1));
    }

    ~CloudStreamSocketShutdown()
    {
        CrossNatConnectorFactory::instance().setCustomFunc(
            std::move(m_oldFactory));
        if (m_clientSocket)
        {
            m_clientSocket->pleaseStopSync();
            m_clientSocket.reset();
        }
    }

protected:
    virtual void SetUp() override
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();

        base_type::SetUp();

        SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            url::Builder().setScheme("stun").setEndpoint(serverAddress()));
    }

    void givenSocketConnectedToATcpServer()
    {
        m_clientSocket = std::make_unique<CloudStreamSocket>(
            SocketFactory::tcpServerIpVersion());
        ASSERT_TRUE(m_clientSocket->connect(
            QnUuid::createUuid().toSimpleString(), nx::network::kNoTimeout));
    }

    void startSocketReadThread()
    {
        nx::utils::promise<void> threadStarted;
        m_socketReadThread = nx::utils::thread(
            [this, &threadStarted]
            {
                ASSERT_TRUE(m_clientSocket->setNonBlockingMode(true));

                threadStarted.set_value();
                char readBuffer[16 * 1024];
                while (!m_terminated)
                    m_clientSocket->recv(readBuffer, sizeof(readBuffer));
            });

        threadStarted.get_future().wait();
    }

    void waitForEnterSocketRecv()
    {
        m_socketInRecv.get_future().wait();
    }

    void shutdownSocket()
    {
        m_clientSocket->shutdown();
    }

    void assertThatSocketHasBeenShutdownCorrectly()
    {
        m_terminated = true;
        m_continueReadingSocket.set_value(true);
        m_socketReadThread.join();
    }

private:
    std::unique_ptr<CloudStreamSocket> m_clientSocket;
    nx::utils::thread m_socketReadThread;
    std::atomic<bool> m_terminated;
    CrossNatConnectorFactory::Function m_oldFactory;
    nx::utils::promise<void> m_socketInRecv;
    nx::utils::promise<bool> m_continueReadingSocket;

    std::unique_ptr<AbstractCrossNatConnector> connectorFactoryFunc(
        const AddressEntry& /*targetAddress*/)
    {
        return std::make_unique<TestCrossNatConnector>(
            serverAddress(), &m_socketInRecv, &m_continueReadingSocket);
    }
};

TEST_F(CloudStreamSocketShutdown, test)
{
    givenSocketConnectedToATcpServer();
    startSocketReadThread();
    waitForEnterSocketRecv();
    shutdownSocket();
    assertThatSocketHasBeenShutdownCorrectly();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
