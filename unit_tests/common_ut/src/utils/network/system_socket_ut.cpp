
#include <thread>

#include <gtest/gtest.h>

#include <utils/common/cpp14.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>

#include "simple_socket_test_helper.h"

TEST( TcpSocket, KeepAliveOptions )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    const auto socket = std::make_unique< TCPSocket >( false );
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
    const auto socket = std::make_unique< TCPSocket >( false );
    boost::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

static const QByteArray kTestMessage("Ping");
static const size_t kClientCount(3);

TEST(TcpSocket, SimpleSync)
{
    socketSimpleSync<TCPServerSocket, TCPSocket>(
        SocketAddress("localhost:12345"),
        kTestMessage,
        kClientCount);
}

TEST(TcpSocket, SimpleAsync)
{
    socketSimpleAsync<TCPServerSocket, TCPSocket>(
        SocketAddress("localhost:12345"),
        kTestMessage,
        kClientCount,
        [this](std::unique_ptr<QnStoppableAsync> socket){
            socket->pleaseStopSync();
        });
}

TEST(TcpSocket, SimpleTrueAsync)
{
    socketSimpleTrueAsync<TCPServerSocket, TCPSocket>(
        SocketAddress("localhost:12345"),
        kTestMessage,
        kClientCount);
}
