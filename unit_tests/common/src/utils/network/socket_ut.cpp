/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <condition_variable>
#include <deque>
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
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }
};

/*!
    This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST_F( SocketAsyncModeTest, AsyncOperationCancellation )
{
    static const int TEST_DURATION_SECONDS = 1;
    static const int TEST_RUNS = 5;

    for( int i = 0; i < TEST_RUNS; ++i )
    {
        static const int MAX_SIMULTANEOUS_CONNECTIONS = 100;
        static const int BYTES_TO_SEND_THROUGH_CONNECTION = 1*1024*1024;

        RandomDataTcpServer server( BYTES_TO_SEND_THROUGH_CONNECTION );
        ASSERT_TRUE( server.start() );

        ConnectionsGenerator connectionsGenerator(
            SocketAddress( QString::fromLatin1("localhost"), server.addressBeingListened().port ),
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
    static const int TEST_RUNS = 7;

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

namespace
{
    //TODO #ak consult gtest docs on how to move it to the test

    static const int CONCURRENT_CONNECTIONS = 100;
    static const int TOTAL_CONNECTIONS = 10000;

    void startAnotherSocket(
        std::condition_variable& cond,
        std::mutex& mutex,
        std::atomic<int>& startedConnectionsCount,
        std::atomic<int>& completedConnectionsCount,
        std::deque<std::unique_ptr<AbstractStreamSocket>>& connections )
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        AbstractStreamSocket* connectionPtr = connection.get();
        connections.push_back( std::move(connection) );
        ASSERT_TRUE( connectionPtr->setNonBlockingMode( true ) );
        ASSERT_TRUE( connectionPtr->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx_http::DEFAULT_HTTP_PORT),
            [&cond, &mutex, &startedConnectionsCount, &completedConnectionsCount, &connections, connectionPtr]
                (SystemError::ErrorCode errorCode) mutable {
                    std::unique_lock<std::mutex> lk( mutex );
                    ASSERT_TRUE( errorCode == SystemError::noError );
                    cond.notify_all();
                    //resolvedAddress = connectionPtr->getForeignAddress().address;

                    auto iterToRemove = std::remove_if(
                        connections.begin(), connections.end(),
                        [connectionPtr]( const std::unique_ptr<AbstractStreamSocket>& elem ) -> bool
                            { return elem.get() == connectionPtr; } );
                    if( iterToRemove != connections.end() )
                    {
                        ++completedConnectionsCount;
                        connections.erase( iterToRemove, connections.end() );
                    }

                    while( connections.size() < CONCURRENT_CONNECTIONS )
                    {
                        if( (++startedConnectionsCount) <= TOTAL_CONNECTIONS )
                            startAnotherSocket(
                                cond,
                                mutex,
                                startedConnectionsCount,
                                completedConnectionsCount,
                                connections );
                        else
                            break;
                    }
                }
            ) );
    }
}

TEST_F( SocketAsyncModeTest, HostNameResolve2 )
{
    std::condition_variable cond;
    std::mutex mutex;
    std::atomic<int> startedConnectionsCount = 0;
    std::atomic<int> completedConnectionsCount = 0;
    std::deque<std::unique_ptr<AbstractStreamSocket>> connections;
    HostAddress resolvedAddress;

    startedConnectionsCount = CONCURRENT_CONNECTIONS;
    for( int i = 0; i < CONCURRENT_CONNECTIONS; ++i )
        startAnotherSocket(
            cond,
            mutex,
            startedConnectionsCount,
            completedConnectionsCount,
            connections );

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    std::unique_lock<std::mutex> lk( mutex );
    while( (completedConnectionsCount + cancelledConnectionsCount) < TOTAL_CONNECTIONS )
    {
        std::unique_ptr<AbstractStreamSocket> connectionToCancel;
        if( !connections.empty() && (canCancelIndex < startedConnectionsCount) )
        {
            auto connectionToCancelIter = connections.begin();
            size_t index = rand() % connections.size();
            std::advance( connectionToCancelIter, index );
            connectionToCancel = std::move(*connectionToCancelIter);
            connections.erase( connectionToCancelIter );
            canCancelIndex += /*index +*/ 1;
        }
        lk.unlock();
        if( connectionToCancel )
        {
            connectionToCancel->cancelAsyncIO();
            connectionToCancel.reset();
            ++cancelledConnectionsCount;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds(1) );
        lk.lock();
    }

    QString ipStr = HostAddress( resolvedAddress.inAddr() ).toString();
    ASSERT_TRUE( connections.empty() );
}

TEST_F( SocketAsyncModeTest, HostNameResolve3 )
{
    {
        nx_http::HttpClient httpClient;
        ASSERT_TRUE( httpClient.doGet( QUrl( "http://ya.ru" ) ) );
        ASSERT_TRUE( httpClient.response() != nullptr );
    }

    {
        nx_http::HttpClient httpClient;
        ASSERT_TRUE( !httpClient.doGet( QUrl( "http://hren2349jf234.ru" ) ) );
    }
}

TEST_F( SocketAsyncModeTest, HostNameResolveCancellation )
{
    static const int TEST_RUNS = 100;

    for( int i = 0; i < TEST_RUNS; ++i )
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        SystemError::ErrorCode connectErrorCode = SystemError::noError;
        std::condition_variable cond;
        std::mutex mutex;
        bool done = false;
        HostAddress resolvedAddress;
        ASSERT_TRUE( connection->setNonBlockingMode( true ) );
        ASSERT_TRUE( connection->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx_http::DEFAULT_HTTP_PORT),
            [&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection](SystemError::ErrorCode errorCode) mutable {
                std::unique_lock<std::mutex> lk( mutex );
                connectErrorCode = errorCode;
                cond.notify_all();
                done = true;
                resolvedAddress = connection->getForeignAddress().address;
            } ) );
        connection->cancelAsyncIO();
    }
}
