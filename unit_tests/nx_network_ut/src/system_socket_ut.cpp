
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>


namespace nx {
namespace network {
namespace test {

TEST( TcpSocket, KeepAliveOptions )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    const auto socket = std::make_unique< TCPSocket >(AF_INET );
    boost::optional< KeepAliveOptions > result;
    result.is_initialized();

    // Enable
    ASSERT_TRUE( socket->setKeepAlive( KeepAliveOptions( 5, 1, 3 ) ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_TRUE( static_cast< bool >( result ) );

    #if defined( Q_OS_LINUX )
        EXPECT_EQ( result->timeSec, 5 );
        EXPECT_EQ( result->intervalSec, 1 );
        EXPECT_EQ( result->probeCount, 3 );
    #elif defined( Q_OS_WIN )
        EXPECT_EQ( result->timeSec, 5 );
        EXPECT_EQ( result->intervalSec, 1 );
        EXPECT_EQ( result->probeCount, 0 ); // means default
    #else
        EXPECT_EQ( result->timeSec, 0 ); // means default
        EXPECT_EQ( result->intervalSec, 0 ); // means default
        EXPECT_EQ( result->probeCount, 0 ); // means default
    #endif

    // Disable
    ASSERT_TRUE( socket->setKeepAlive( boost::none ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

TEST(TcpSocket, DISABLED_KeepAliveOptionsDefaults)
{
    const auto socket = std::make_unique< TCPSocket >(AF_INET );
    boost::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

static void waitForKeepAliveDisconnect(AbstractStreamSocket* socket)
{
    Buffer buffer(1024, Qt::Uninitialized);
    ASSERT_TRUE(socket->setKeepAlive(KeepAliveOptions(10, 5, 3)));

    NX_LOG(lm("waitForKeepAliveDisconnect recv"), cl_logINFO);
    EXPECT_LT(socket->recv(buffer.data(), buffer.size()), 0);
    EXPECT_NE(SystemError::noError, SystemError::getLastOSErrorCode());
    NX_LOG(lm("waitForKeepAliveDisconnect end"), cl_logINFO);
}

TEST(TcpSocket, DISABLED_KeepAliveOptionsServer)
{
    const auto server = std::make_unique<TCPServerSocket>(AF_INET);
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
    ASSERT_TRUE(server->listen(testClientCount()));
    NX_LOGX(lm("Server address: %1").str(server->getLocalAddress()), cl_logINFO);

    std::unique_ptr<AbstractStreamSocket> client(server->accept());
    ASSERT_TRUE(client);
    waitForKeepAliveDisconnect(client.get());
}

TEST(TcpSocket, KeepAliveOptionsClient)
{
    const auto client = std::make_unique<TCPSocket>(AF_INET);
    ASSERT_TRUE(client->connect(SocketAddress("52.55.219.5:3345")));
    waitForKeepAliveDisconnect(client.get());
}

TEST(TcpSocket, ErrorHandling)
{
    nx::network::test::socketErrorHandling(
        []() { return std::make_unique<TCPServerSocket>(AF_INET); },
        []() { return std::make_unique<TCPSocket>(AF_INET); });
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

// TODO: Figure out why it does not work for any local address:
NX_NETWORK_TRANSMIT_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV4to6,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET6); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); })

// TODO: Figure out why it does not work for any local address:
NX_NETWORK_TRANSMIT_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV6to4,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET); },
    [](){ return std::make_unique<TCPSocket>(AF_INET6); })

}   //test
}   //network
}   //nx
