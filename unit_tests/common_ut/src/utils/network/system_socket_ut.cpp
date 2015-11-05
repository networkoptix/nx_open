#include <gtest/gtest.h>

#include <utils/network/socket_factory.h>

TEST(TcpSocket, KeepAliveOptions)
{
    const auto socket = SocketFactory::createStreamSocket();
    ASSERT_TRUE( socket->setKeepAlive( KeepAliveOptions( 5, 1, 3 ) ) );

    boost::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_TRUE( static_cast< bool >( result ) );
    ASSERT_EQ( result->timeSec, 5 );

    #if defined( Q_OS_WIN ) || defined( Q_OS_LINUX )
        ASSERT_EQ( result->intervalSec, 1 );
        ASSERT_EQ( result->probeCount, 3 );
    #endif
}
