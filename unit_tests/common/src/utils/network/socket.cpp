/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <chrono>

#include <gtest/gtest.h>


#if 0
/*!
    This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST( SocketAsyncModeTest, AsyncOperationCancellation )
{
    static const int MAX_SIMULTANEOUS_CONNECTIONS = 100;
    static const int BYTES_TO_SEND_THROUGH_CONNECTION = 100*1024*1024;
    static const int TEST_DURATION_SECONDS = 11;

    RandomDataTcpServer server( BYTES_TO_SEND_THROUGH_CONNECTION );
    ASSERT( server.start() );

    ConnectionsGenerator connectionsGenerator(
        server.addressBeingListened(),
        MAX_SIMULTANEOUS_CONNECTIONS,
        BYTES_TO_SEND_THROUGH_CONNECTION );
    ASSERT( connectionsGenerator.start() );

    std::this_thread::sleep_for( std::chrono::seconds( TEST_DURATION_SECONDS ) );

    connectionsGenerator.pleaseStop();
    connectionsGenerator.join();

    server.pleaseStop();
    server.join();
}
#endif
