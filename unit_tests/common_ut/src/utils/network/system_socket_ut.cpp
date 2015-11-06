#include <gtest/gtest.h>

#include <utils/network/socket_factory.h>

TEST(TcpSocket, KeepAliveOptions)
{
    const auto socket = SocketFactory::createStreamSocket();
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
    const auto socket = SocketFactory::createStreamSocket();
    boost::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}
