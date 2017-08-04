#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

#include "common_server_socket_ut.h"

namespace nx {
namespace network {
namespace test {

class TcpSocket:
    public ::testing::Test
{
public:
    ~TcpSocket()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    void whenAcceptedSocket()
    {
        m_client = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_client->setNonBlockingMode(true));

        nx::utils::promise<SystemError::ErrorCode> connected;
        m_client->connectAsync(
            m_tcpServerSocket.getLocalAddress(),
            [&connected](SystemError::ErrorCode resultCode)
            {
                connected.set_value(resultCode);
            });

         m_acceptedSocket.reset(m_tcpServerSocket.accept());
         ASSERT_EQ(SystemError::noError, connected.get_future().get());
    }

    void thenTimeoutsAreInheritedFromTheServerSocket()
    {
        thenRecvTimeoutIsInherited();
        thenSendTimeoutIsInherited();
    }

    void thenRecvTimeoutIsInherited()
    {
        unsigned int acceptedSocketRecvTimeout = 0;
        ASSERT_TRUE(m_acceptedSocket->getRecvTimeout(&acceptedSocketRecvTimeout));
        unsigned int serverSocketRecvTimeout = 0;
        ASSERT_TRUE(m_tcpServerSocket.getRecvTimeout(&serverSocketRecvTimeout));
        ASSERT_EQ(serverSocketRecvTimeout, acceptedSocketRecvTimeout);
    }

    void thenSendTimeoutIsInherited()
    {
        unsigned int acceptedSocketSendTimeout = 0;
        ASSERT_TRUE(m_acceptedSocket->getSendTimeout(&acceptedSocketSendTimeout));
        unsigned int serverSocketSendTimeout = 0;
        ASSERT_TRUE(m_tcpServerSocket.getSendTimeout(&serverSocketSendTimeout));
        ASSERT_EQ(serverSocketSendTimeout, acceptedSocketSendTimeout);
    }

private:
    std::unique_ptr<TCPSocket> m_client;
    TCPServerSocket m_tcpServerSocket;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_tcpServerSocket.setRecvTimeout(
            nx::utils::random::number<unsigned int>(10000, 90000)));
        ASSERT_TRUE(m_tcpServerSocket.setSendTimeout(
            nx::utils::random::number<unsigned int>(10000, 90000)));

        ASSERT_TRUE(m_tcpServerSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_tcpServerSocket.listen());
    }
};

TEST_F(TcpSocket, KeepAliveOptions )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    const auto socket = std::make_unique< TCPSocket >(AF_INET);
    boost::optional< KeepAliveOptions > result;

    // Enable
    typedef std::chrono::seconds seconds;
    ASSERT_TRUE( socket->setKeepAlive( KeepAliveOptions( seconds(5), seconds(1), 3 ) ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_TRUE( static_cast< bool >( result ) );

    #if defined( Q_OS_LINUX )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe.count(), 5 );
        EXPECT_EQ( result->probeSendPeriod.count(), 1 );
        EXPECT_EQ( result->probeCount, 3U );
    #elif defined( Q_OS_WIN )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe.count(), 5 );
        EXPECT_EQ( result->probeSendPeriod.count(), 1 );
        EXPECT_EQ( result->probeCount, 0U ); // means default
    #elif defined( Q_OS_MACX )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe.count(), 5 );
        EXPECT_EQ( result->probeSendPeriod.count(), 0 ); // means default
        EXPECT_EQ( result->probeCount, 0U ); // means default
    #endif

    // Disable
    ASSERT_TRUE( socket->setKeepAlive( boost::none ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

TEST_F(TcpSocket, DISABLED_KeepAliveOptionsDefaults)
{
    const auto socket = std::make_unique< TCPSocket >( AF_INET );
    boost::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

static void waitForKeepAliveDisconnect(AbstractStreamSocket* socket)
{
    Buffer buffer(1024, Qt::Uninitialized);
    ASSERT_TRUE(socket->setKeepAlive(KeepAliveOptions(
        std::chrono::seconds(10), std::chrono::seconds(5), 3)));

    NX_LOG(lm("waitForKeepAliveDisconnect recv"), cl_logINFO);
    EXPECT_LT(socket->recv(buffer.data(), buffer.size()), 0);
    EXPECT_NE(SystemError::noError, SystemError::getLastOSErrorCode());
    NX_LOG(lm("waitForKeepAliveDisconnect end"), cl_logINFO);
}

TEST_F(TcpSocket, DISABLED_KeepAliveOptionsServer)
{
    const auto server = std::make_unique<TCPServerSocket>(AF_INET);
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
    ASSERT_TRUE(server->listen(testClientCount()));
    NX_LOGX(lm("Server address: %1").arg(server->getLocalAddress()), cl_logINFO);

    std::unique_ptr<AbstractStreamSocket> client(server->accept());
    ASSERT_TRUE(client != nullptr);
    waitForKeepAliveDisconnect(client.get());
}

TEST_F(TcpSocket, DISABLED_KeepAliveOptionsClient)
{
    const auto client = std::make_unique<TCPSocket>(AF_INET);
    ASSERT_TRUE(client->connect(SocketAddress("52.55.219.5:3345")));
    waitForKeepAliveDisconnect(client.get());
}

TEST_F(TcpSocket, ErrorHandling)
{
    nx::network::test::socketErrorHandling(
        []() { return std::make_unique<TCPServerSocket>(AF_INET); },
        []() { return std::make_unique<TCPSocket>(AF_INET); });
}

TEST_F(TcpSocket, socket_timer_is_single_shot)
{
    constexpr auto kTimeout = std::chrono::milliseconds(10);

    std::atomic<int> triggerCount(0);

    TCPSocket tcpSocket(AF_INET);
    tcpSocket.registerTimer(kTimeout, [&triggerCount]() { ++triggerCount; });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    tcpSocket.pleaseStopSync();

    ASSERT_EQ(1, triggerCount.load());
}

TEST_F(TcpSocket, ConnectErrorReporting)
{
    // Connecting to port 1 which is reserved for now deprecated TCPMUX service.
    // So, in most cases can expect port 1 to be unused.
    const SocketAddress localAddress(HostAddress::localhost, 1);

    TCPSocket tcpSocket(AF_INET);
    const bool connectResult = tcpSocket.connect(localAddress, 1500);
    const auto connectErrorCode = SystemError::getLastOSErrorCode();
    ASSERT_FALSE(connectResult);
    ASSERT_NE(SystemError::noError, connectErrorCode);
    ASSERT_FALSE(tcpSocket.isConnected());
}

TEST_F(TcpSocket, accepted_socket_inherits_timeouts_from_server_socket)
{
    whenAcceptedSocket();
    thenTimeoutsAreInheritedFromTheServerSocket();
}

TEST(TcpServerSocketIpv6, BindsToLocalAddress)
{
    TCPServerSocket socket(AF_INET6);
    ASSERT_TRUE(socket.bind(SocketAddress::anyPrivateAddress));
    ASSERT_EQ(HostAddress::localhost, socket.getLocalAddress().address);
}

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, TcpSocketV4,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); })

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, TcpSocketV6,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET6); },
    [](){ return std::make_unique<TCPSocket>(AF_INET6); })

// NOTE: Currenly IP v4 address 127.0.0.1 is represented as ::1 IP v6 address, which is not true.
// TODO: Enable these tests when IP v6 is properly supported.
NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV4to6,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET6); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); })

NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV6to4,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET); },
    [](){ return std::make_unique<TCPSocket>(AF_INET6); })

INSTANTIATE_TYPED_TEST_CASE_P(TCPServerSocket, ServerSocketTest, TCPServerSocket);

}   //test
}   //network
}   //nx
