#include <chrono>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

NX_NETWORK_CLIENT_SOCKET_TEST_CASE(
    TEST, CloudStreamSocketTcpByIp,
    []() { return std::make_unique<TCPServerSocket>(AF_INET); },
    []() { return std::make_unique<CloudStreamSocket>(AF_INET); });

TEST(CloudStreamSocketTcpByIp, SimpleSyncSsl)
{
    network::test::socketSimpleSync(
        [&]() { return std::make_unique<SslServerSocket>(new TCPServerSocket(AF_INET), false); },
        []() { return std::make_unique<SslSocket>(new CloudStreamSocket(AF_INET), false); });
}

class TestTcpServerSocket:
    public TCPServerSocket
{
public:
    TestTcpServerSocket(HostAddress host):
        TCPServerSocket(AF_INET),
        m_host(std::move(host))
    {
    }

    ~TestTcpServerSocket()
    {
        if (m_socketAddress)
            SocketGlobals::addressResolver().removeFixedAddress(m_host, *m_socketAddress);
    }

    virtual bool bind(const SocketAddress& localAddress) override
    {
        if (!TCPServerSocket::bind(localAddress))
            return false;

        m_socketAddress = TCPServerSocket::getLocalAddress();
        SocketGlobals::addressResolver().addFixedAddress(m_host, *m_socketAddress);
        return true;
    }

    virtual SocketAddress getLocalAddress() const override
    {
        return m_socketAddress ? *m_socketAddress : SocketAddress();
    }

private:
    const HostAddress m_host;
    boost::optional<SocketAddress> m_socketAddress;
};

struct CloudStreamSocketTcpByHost:
    public ::testing::Test
{
    const HostAddress testHost;
    CloudStreamSocketTcpByHost(): testHost(id() + lit(".") + id()) {}
    static QString id() { return QnUuid::createUuid().toSimpleString(); }
};

NX_NETWORK_CLIENT_SOCKET_TEST_CASE_EX(
    TEST_F, CloudStreamSocketTcpByHost,
    [&]() { return std::make_unique<TestTcpServerSocket>(testHost); },
    [&]() { return std::make_unique<CloudStreamSocket>(AF_INET); },
    SocketAddress(testHost));

TEST_F(CloudStreamSocketTcpByHost, SimpleSyncSsl)
{
    network::test::socketSimpleSync(
        [&]() { return std::make_unique<SslServerSocket>(new TestTcpServerSocket(testHost), false); },
        []() { return std::make_unique<SslSocket>(new CloudStreamSocket(AF_INET), false); },
        SocketAddress(testHost));
}

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

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(SocketAddress(tempHostName), 0));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
        ASSERT_EQ(bytesToSendThroughConnection, (size_t)bytesRead);
    }

    // also try to connect just by system name
    {
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(SocketAddress("bla"), 0));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
        ASSERT_EQ(bytesToSendThroughConnection, (size_t)bytesRead);
    }

    server.pleaseStopSync();
    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

TEST_F(CloudStreamSocketTest, multiple_connections_random_data)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t maxSimultaneousConnections = 1000;
    const std::chrono::seconds testDuration(3);

    setCreateStreamSocketFunc(
        []( bool /*sslRequired*/,
            nx::network::NatTraversalSupport /*natTraversalRequired*/) ->
                std::unique_ptr< AbstractStreamSocket >
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
        SocketAddress(tempHostName),
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

    const auto scopedGuard = makeScopedGuard(
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
            SocketAddress(tempHostName),
            [](SystemError::ErrorCode /*code*/){});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    //cancelling read
    for (size_t i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket(AF_INET);
        ASSERT_TRUE(cloudSocket.connect(
            SocketAddress(tempHostName),
            std::chrono::milliseconds(1000).count()))
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
        ASSERT_TRUE(cloudSocket.connect(
            SocketAddress(tempHostName),
            std::chrono::milliseconds(1000).count()))
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

TEST_F(CloudStreamSocketTest, syncModeCancellation)
{
    constexpr const std::chrono::milliseconds kTestDuration =
        std::chrono::milliseconds(2000);
    constexpr const int kTestRuns = 10;

    //launching some server
    network::test::RandomDataTcpServer tcpServer(
        network::test::TestTrafficLimitType::none,
        0,
        network::test::TestTransmissionMode::spam);
    tcpServer.setLocalAddress(SocketAddress(HostAddress::localhost, 0));
    ASSERT_TRUE(tcpServer.start());

    for (int i = 0; i < kTestRuns; ++i)
    {
        auto socket = std::make_unique<CloudStreamSocket>(SocketFactory::tcpServerIpVersion());
        enum class SocketState {init, connected, closed};
        std::atomic<SocketState> socketState(SocketState::init);

        nx::utils::thread sendThread(
            [&socket, targetAddress = tcpServer.addressBeingListened(), &socketState]
            {
                char testBuffer[16*1024];

                if (!socket->isConnected())
                {
                    ASSERT_TRUE(socket->connect(targetAddress, 3000));
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

    tcpServer.pleaseStopSync();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
