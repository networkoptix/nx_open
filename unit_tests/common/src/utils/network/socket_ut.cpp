/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "socket_test_helper.h"


namespace 
{
    const int SECONDS_TO_WAIT_AFTER_TEST = 5;
}

/*!
    This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST( SocketAsyncModeTest, AsyncOperationCancellation )
{
    static const int TEST_DURATION_SECONDS = 2;
    static const int TEST_RUNS = 5;

    for( int i = 0; i < TEST_RUNS; ++i )
    {
        static const int MAX_SIMULTANEOUS_CONNECTIONS = 100;
        static const int BYTES_TO_SEND_THROUGH_CONNECTION = 1*1024*1024;

        RandomDataTcpServer server( BYTES_TO_SEND_THROUGH_CONNECTION );
        ASSERT_TRUE( server.start() );

        ConnectionsGenerator connectionsGenerator(
            server.addressBeingListened(),
            MAX_SIMULTANEOUS_CONNECTIONS,
            BYTES_TO_SEND_THROUGH_CONNECTION );
        ASSERT_TRUE( connectionsGenerator.start() );

        std::this_thread::sleep_for( std::chrono::seconds( TEST_DURATION_SECONDS ) );

        connectionsGenerator.pleaseStop();
        connectionsGenerator.join();

        server.pleaseStop();
        server.join();
    }

    //waiting for some calls to deleted objects
    std::this_thread::sleep_for( std::chrono::seconds( SECONDS_TO_WAIT_AFTER_TEST ) );
}

TEST( SocketAsyncModeTest, ServerSocketAsyncCancellation )
{
    static const int TEST_RUNS = 47;

    for( int i = 0; i < TEST_RUNS; ++i )
    {
        std::unique_ptr<AbstractStreamServerSocket> serverSocket( SocketFactory::createStreamServerSocket() );
        ASSERT_TRUE( serverSocket->setNonBlockingMode(true) );
        ASSERT_TRUE( serverSocket->bind(SocketAddress()) );
        ASSERT_TRUE( serverSocket->listen() );
        ASSERT_TRUE( serverSocket->acceptAsync( [](SystemError::ErrorCode, AbstractStreamSocket*){  } ) );
        serverSocket->terminateAsyncIO( true );
    }

    //waiting for some calls to deleted objects
    std::this_thread::sleep_for( std::chrono::seconds( SECONDS_TO_WAIT_AFTER_TEST ) );
}
