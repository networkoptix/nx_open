/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include <utils/network/dns_resolver.h>
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
};

class HostNameResolveTest
:
    public SocketAsyncModeTest
{
public:
    HostNameResolveTest()
    :
        m_startedConnectionsCount( 0 ),
        m_completedConnectionsCount( 0 )
    {
    }

protected:
    std::condition_variable m_cond;
    std::mutex m_mutex;
    std::atomic<int> m_startedConnectionsCount;
    std::atomic<int> m_completedConnectionsCount;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_connections;

    void startAnotherSocketNonSafe()
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        AbstractStreamSocket* connectionPtr = connection.get();
        m_connections.push_back( std::move(connection) );
        ASSERT_TRUE( connectionPtr->setNonBlockingMode( true ) );
        ASSERT_TRUE( connectionPtr->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx_http::DEFAULT_HTTP_PORT),
            std::bind( &HostNameResolveTest::onConnectionComplete, this, connectionPtr, std::placeholders::_1 ) ) );
    }

    void onConnectionComplete(
        AbstractStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode )
    {
        ASSERT_TRUE( errorCode == SystemError::noError );

        std::unique_lock<std::mutex> lk( m_mutex );
        m_cond.notify_all();
        //resolvedAddress = connectionPtr->getForeignAddress().address;

        auto iterToRemove = std::remove_if(
            m_connections.begin(), m_connections.end(),
            [connectionPtr]( const std::unique_ptr<AbstractStreamSocket>& elem ) -> bool
                { return elem.get() == connectionPtr; } );
        if( iterToRemove != m_connections.end() )
        {
            ++m_completedConnectionsCount;
            m_connections.erase( iterToRemove, m_connections.end() );
        }

        while( m_connections.size() < CONCURRENT_CONNECTIONS )
        {
            if( (++m_startedConnectionsCount) <= TOTAL_CONNECTIONS )
                startAnotherSocketNonSafe();
            else
                break;
        }
    }


    static const int CONCURRENT_CONNECTIONS = 30;
    static const int TOTAL_CONNECTIONS = 3000;

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

        QThread::sleep( TEST_DURATION_SECONDS );

        connectionsGenerator.pleaseStop();
        connectionsGenerator.join();

        server.pleaseStop();
        server.join();
    }

    //waiting for some calls to deleted objects
    QThread::sleep( SECONDS_TO_WAIT_AFTER_TEST );
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
    QThread::sleep( SECONDS_TO_WAIT_AFTER_TEST );
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

TEST_F( HostNameResolveTest, HostNameResolve2 )
{
    HostAddress resolvedAddress;

    m_startedConnectionsCount = CONCURRENT_CONNECTIONS;
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        for( int i = 0; i < CONCURRENT_CONNECTIONS; ++i )
            startAnotherSocketNonSafe();
    }

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    std::unique_lock<std::mutex> lk( m_mutex );
    while( (m_completedConnectionsCount + cancelledConnectionsCount) < TOTAL_CONNECTIONS )
    {
        std::unique_ptr<AbstractStreamSocket> connectionToCancel;
        if( m_connections.size() > 1 && (canCancelIndex < m_startedConnectionsCount) )
        {
            auto connectionToCancelIter = m_connections.begin();
            size_t index = rand() % m_connections.size();
            std::advance( connectionToCancelIter, index );
            connectionToCancel = std::move(*connectionToCancelIter);
            m_connections.erase( connectionToCancelIter );
            canCancelIndex += /*index +*/ 1;
        }
        lk.unlock();
        if( connectionToCancel )
        {
            connectionToCancel->cancelAsyncIO();
            connectionToCancel.reset();
            ++cancelledConnectionsCount;
        }
        QThread::msleep( 1 );
        lk.lock();
    }

    QString ipStr = HostAddress( resolvedAddress.inAddr() ).toString();
    ASSERT_TRUE( m_connections.empty() );
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

TEST_F( SocketAsyncModeTest, BadHostNameResolve )
{
    static const int TEST_RUNS = 10000;

    for( int i = 0; i < TEST_RUNS; ++i )
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        //SystemError::ErrorCode connectErrorCode = SystemError::noError;
        //std::condition_variable cond;
        //std::mutex mutex;
        //bool done = false;
        //HostAddress resolvedAddress;
        int iBak = i;
        ASSERT_TRUE( connection->setNonBlockingMode( true ) );
        ASSERT_TRUE( connection->connectAsync(
            SocketAddress( QString::fromLatin1( "hx.hz" ), nx_http::DEFAULT_HTTP_PORT ),
            [&i, iBak/*&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection*/]
            ( SystemError::ErrorCode /*errorCode*/ ) mutable {
                //std::unique_lock<std::mutex> lk( mutex );
                //connectErrorCode = errorCode;
                //cond.notify_all();
                //done = true;
                //resolvedAddress = connection->getForeignAddress().address;
                ASSERT_EQ( i, iBak );
                int x = 0;
                static_cast< void >( x );
            } ) );
        connection->cancelAsyncIO();
    }
}

#if 0
TEST_F( SocketAsyncModeTest, postCancellation )
{
    static const int TEST_RUNS = 200;

    std::atomic<size_t> postCalls( 0 );

    auto testFunctor = [&postCalls]()
    {
        std::atomic<size_t> counter( 0 );

        for( int i = 0; i < TEST_RUNS; ++i )
        {
            size_t curCounterVal = ++counter;

            std::vector<std::unique_ptr<AbstractStreamSocket>> sockets( 50 );
            std::for_each(
                sockets.begin(),
                sockets.end(),
                []( std::unique_ptr<AbstractStreamSocket>& ptr ){ ptr.reset( SocketFactory::createStreamSocket() ); } );
            //std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );

            for( const auto& sock: sockets )
                sock->post( [curCounterVal, &counter, &postCalls]() {
                    ASSERT_EQ( curCounterVal, (size_t)counter );
                    ++postCalls;
                    QThread::usleep( 10 );
                } );

            for( const auto& sock : sockets )
                sock->cancelAsyncIO();

            //QThread::usleep( 100 );
        }
    };

    std::vector<std::future<void>> futures;
    for( int i = 0; i < 25; ++i )
        futures.emplace_back( std::async( std::launch::async, testFunctor ) );

    for( auto& f: futures )
        f.wait();
}
#endif
