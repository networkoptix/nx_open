/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <utils/network/host_address_resolver.h>
#include <utils/network/http/httpclient.h>

#include "socket_test_helper.h"


namespace 
{
    const int SECONDS_TO_WAIT_AFTER_TEST = 5;
}

class SocketAsyncModeTest
:
    public ::testing::Test
{
protected:
    static std::unique_ptr<SocketGlobalRuntime> socketGlobalRuntimeInstance;

    static void SetUpTestCase()
    {
        socketGlobalRuntimeInstance.reset( new SocketGlobalRuntime() );
    }

    static void TearDownTestCase()
    {
        socketGlobalRuntimeInstance.reset();
    }
};

std::unique_ptr<SocketGlobalRuntime> SocketAsyncModeTest::socketGlobalRuntimeInstance;

/*!
    This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST_F( SocketAsyncModeTest, AsyncOperationCancellation )
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

TEST_F( SocketAsyncModeTest, ServerSocketAsyncCancellation )
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

TEST_F( SocketAsyncModeTest, HostNameResolve1 )
{
    std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
    SystemError::ErrorCode connectErrorCode = SystemError::noError;
    std::condition_variable cond;
    std::mutex mutex;
    bool done = false;
    HostAddress resolvedAddress;
    ASSERT_TRUE( connection->setNonBlockingMode( true ) );
    ASSERT_TRUE( connection->connectAsync(
        SocketAddress(QString::fromLatin1("ya.ru"), 80),
        [&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection](SystemError::ErrorCode errorCode) mutable {
            std::unique_lock<std::mutex> lk( mutex );
            connectErrorCode = errorCode;
            cond.notify_all();
            done = true;
            resolvedAddress = connection->getForeignAddress().address;
        } ) );

    std::unique_lock<std::mutex> lk( mutex );
    while( !done )
        cond.wait( lk );

    QString ipStr = HostAddress( resolvedAddress.inAddr() ).toString();

    ASSERT_TRUE( connectErrorCode == SystemError::noError );
}

TEST_F( SocketAsyncModeTest, HostNameResolve2 )
{
    nx_http::HttpClient httpClient;
    ASSERT_TRUE( httpClient.doGet( QUrl( "http://ya.ru" ) ) );
    ASSERT_TRUE( httpClient.response() != nullptr );
}

TEST_F( SocketAsyncModeTest, HostNameResolveCancellation )
{
    std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
    SystemError::ErrorCode connectErrorCode = SystemError::noError;
    std::condition_variable cond;
    std::mutex mutex;
    bool done = false;
    HostAddress resolvedAddress;
    ASSERT_TRUE( connection->setNonBlockingMode( true ) );
    ASSERT_TRUE( connection->connectAsync(
        SocketAddress(QString::fromLatin1("ya.ru"), 80),
        [&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection](SystemError::ErrorCode errorCode) mutable {
            std::unique_lock<std::mutex> lk( mutex );
            connectErrorCode = errorCode;
            cond.notify_all();
            done = true;
            resolvedAddress = connection->getForeignAddress().address;
        } ) );
    connection->cancelAsyncIO();
    connection.reset();
}
