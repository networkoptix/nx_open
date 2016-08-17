/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include <nx/network/dns_resolver.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace network {
namespace test {

namespace 
{
    const int SECONDS_TO_WAIT_AFTER_TEST = 5;
}

class SocketHostNameResolveTest
:
    public testing::Test
{
public:
    SocketHostNameResolveTest()
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
        connectionPtr->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx_http::DEFAULT_HTTP_PORT),
            std::bind( &SocketHostNameResolveTest::onConnectionComplete, this, connectionPtr, std::placeholders::_1 ) );
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
TEST( Socket, AsyncOperationCancellation )
{
    static const std::chrono::milliseconds TEST_DURATION(
        300 * nx::utils::TestOptions::timeoutMultiplier());

    static const int kTestRuns = 5;
    static const int THREADS = 3;

    std::vector<nx::utils::thread> threads(THREADS);
    for (size_t i = 0; i < threads.size(); ++i)
    {
        threads[i] = nx::utils::thread(
            [](){
                for (int j = 0; j < kTestRuns; ++j)
                {
                    static const int MAX_SIMULTANEOUS_CONNECTIONS = 25;
                    static const int BYTES_TO_SEND_THROUGH_CONNECTION = 1 * 1024;

                    RandomDataTcpServer server(
                        TestTrafficLimitType::none,
                        BYTES_TO_SEND_THROUGH_CONNECTION,
                        SocketFactory::isSslEnforced()
                            ? TestTransmissionMode::pong
                            : TestTransmissionMode::spam);
                    ASSERT_TRUE(server.start());

                    ConnectionsGenerator connectionsGenerator(
                        SocketAddress(QString::fromLatin1("localhost"), server.addressBeingListened().port),
                        MAX_SIMULTANEOUS_CONNECTIONS,
                        TestTrafficLimitType::incoming,
                        BYTES_TO_SEND_THROUGH_CONNECTION,
                        ConnectionsGenerator::kInfiniteConnectionCount,
                        TestTransmissionMode::spam);
                    connectionsGenerator.start();

                    std::this_thread::sleep_for(TEST_DURATION);

                    connectionsGenerator.pleaseStopSync();
                    server.pleaseStopSync();

                    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0);
                    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0);
                }
            });
    }

    for (size_t i = 0; i < threads.size(); ++i)
        threads[i].join();

    //waiting for some calls to deleted objects
    QThread::sleep( SECONDS_TO_WAIT_AFTER_TEST );
}

TEST( Socket, ServerSocketSyncCancellation )
{
    static const int kTestRuns = 7;

    for( int i = 0; i < kTestRuns; ++i )
    {
        std::unique_ptr<AbstractStreamServerSocket> serverSocket( SocketFactory::createStreamServerSocket() );
        ASSERT_TRUE( serverSocket->setNonBlockingMode(true) );
        ASSERT_TRUE( serverSocket->bind(SocketAddress()) );
        ASSERT_TRUE( serverSocket->listen() );
        serverSocket->acceptAsync( [](SystemError::ErrorCode, AbstractStreamSocket*){  } );
        serverSocket->pleaseStopSync();
    }

    //waiting for some calls to deleted objects
    QThread::sleep( SECONDS_TO_WAIT_AFTER_TEST );
}

TEST( Socket, ServerSocketAsyncCancellation )
{
    static const int kTestRuns = 7;

    for( int i = 0; i < kTestRuns; ++i )
    {
        std::unique_ptr<AbstractStreamServerSocket> serverSocket( SocketFactory::createStreamServerSocket() );
        ASSERT_TRUE( serverSocket->setNonBlockingMode(true) );
        ASSERT_TRUE( serverSocket->bind(SocketAddress()) );
        ASSERT_TRUE( serverSocket->listen() );
        serverSocket->acceptAsync( [](SystemError::ErrorCode, AbstractStreamSocket*){  } );
        QnStoppableAsync::pleaseStop( [](){}, std::move( serverSocket ) );
    }

    //waiting for some calls to deleted objects
    QThread::sleep( SECONDS_TO_WAIT_AFTER_TEST );
}

TEST( Socket, HostNameResolve1 )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
    SystemError::ErrorCode connectErrorCode = SystemError::noError;
    std::condition_variable cond;
    std::mutex mutex;
    bool done = false;
    HostAddress resolvedAddress;
    ASSERT_TRUE( connection->setNonBlockingMode( true ) );
    connection->connectAsync(
        SocketAddress(QString::fromLatin1("ya.ru"), 80),
        [&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection](SystemError::ErrorCode errorCode) mutable {
            std::unique_lock<std::mutex> lk( mutex );
            connectErrorCode = errorCode;
            cond.notify_all();
            done = true;
            resolvedAddress = connection->getForeignAddress().address;
        } );

    std::unique_lock<std::mutex> lk( mutex );
    while( !done )
        cond.wait( lk );

    QString ipStr = resolvedAddress.toString();
    static_cast<void>( ipStr );
    ASSERT_TRUE( connectErrorCode == SystemError::noError );
}

TEST_F( SocketHostNameResolveTest, HostNameResolve2 )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

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
            size_t index = nx::utils::random::number<size_t>(0, m_connections.size() - 1);
            std::advance( connectionToCancelIter, index );
            connectionToCancel = std::move(*connectionToCancelIter);
            m_connections.erase( connectionToCancelIter );
            canCancelIndex += /*index +*/ 1;
        }
        lk.unlock();
        if( connectionToCancel )
        {
            connectionToCancel->pleaseStopSync();
            connectionToCancel.reset();
            ++cancelledConnectionsCount;
        }
        QThread::msleep( 1 );
        lk.lock();
    }

    QString ipStr = resolvedAddress.toString();
    static_cast<void>( ipStr );
    ASSERT_TRUE( m_connections.empty() );
}

TEST( Socket, HostNameResolve3 )
{
    nx::DnsResolver dnsResolver;

    {
        HostAddress resolvedAddress;
        EXPECT_TRUE(
            dnsResolver.resolveAddressSync(
                QLatin1String("ya.ru"),
                &resolvedAddress) );

        EXPECT_TRUE(resolvedAddress.ipv4() != 0);
        EXPECT_NE(resolvedAddress.ipV4()->s_addr, 0);
    }

    {
        HostAddress resolvedAddress;
        ASSERT_FALSE(
            dnsResolver.resolveAddressSync(
                QLatin1String("hren2349jf234.ru"),
                &resolvedAddress));
    }
}

TEST( Socket, HostNameResolveCancellation )
{
    static const int kTestRuns = 100;

    for( int i = 0; i < kTestRuns; ++i )
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        SystemError::ErrorCode connectErrorCode = SystemError::noError;
        std::condition_variable cond;
        std::mutex mutex;
        bool done = false;
        HostAddress resolvedAddress;
        ASSERT_TRUE( connection->setNonBlockingMode( true ) );
        connection->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx_http::DEFAULT_HTTP_PORT),
            [&connectErrorCode, &done, &resolvedAddress, &cond, &mutex, &connection](
                SystemError::ErrorCode errorCode) mutable
            {
                std::unique_lock<std::mutex> lk( mutex );
                connectErrorCode = errorCode;
                cond.notify_all();
                done = true;
                resolvedAddress = connection->getForeignAddress().address;
            });
        connection->pleaseStopSync();
    }
}

TEST( Socket, BadHostNameResolve )
{
    static const int kTestRuns = 1000;
    for( int i = 0; i < kTestRuns; ++i )
    {
        std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );
        int iBak = i;
        ASSERT_TRUE( connection->setNonBlockingMode( true ) );
        connection->connectAsync(
            SocketAddress( QString::fromLatin1( "hx.hz" ), nx_http::DEFAULT_HTTP_PORT ),
            [&i, iBak](SystemError::ErrorCode /*errorCode*/) mutable
            {
                ASSERT_EQ( i, iBak );
            });
        connection->pleaseStopSync();
    }
}

TEST(Socket, postCancellation)
{
    constexpr static const int kTestRuns = 10;
    constexpr static const int kTestThreadCount = 10;
    constexpr static const int kTestSocketCount = 25;

    std::atomic<size_t> postCalls(0);

    auto testFunctor = [&postCalls]()
    {
        std::atomic<size_t> counter(0);

        for (int i = 0; i < kTestRuns; ++i)
        {
            size_t curCounterVal = ++counter;

            std::vector<std::unique_ptr<AbstractStreamSocket>> sockets(kTestSocketCount);
            std::for_each(
                sockets.begin(),
                sockets.end(),
                [](std::unique_ptr<AbstractStreamSocket>& ptr) { ptr = SocketFactory::createStreamSocket(); });
            //std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );

            for (const auto& sock : sockets)
                sock->post(
                    [curCounterVal, &counter, &postCalls]()
                    {
                        ASSERT_EQ(curCounterVal, (size_t)counter);
                        ++postCalls;
                        QThread::usleep(10);
                    });

            for (const auto& sock : sockets)
                sock->pleaseStopSync();

            //QThread::usleep( 100 );
        }
    };

    std::vector<nx::utils::thread> testThreads;
    for (int i = 0; i < kTestThreadCount; ++i)
        testThreads.emplace_back(nx::utils::thread(testFunctor));

    for (auto& testThread : testThreads)
        testThread.join();
}

void testHostAddress(
    HostAddress host,
    boost::optional<in_addr_t> ipv4,
    boost::optional<in6_addr> ipv6,
    const char* string)
{
    ASSERT_EQ(host.toString(), QString(string));

    if (ipv4)
    {
        const auto ip =  host.ipV4();
        ASSERT_TRUE((bool)ip);
        ASSERT_EQ(ip->s_addr, *ipv4);
    }
    else
    {
        ASSERT_FALSE((bool)host.ipV4());
    }

    if (ipv6)
    {
        const auto ip =  host.ipV6();
        ASSERT_TRUE((bool)ip);
        ASSERT_EQ(memcmp(&ip.get(), &ipv6.get(), sizeof(in6_addr)), 0);
    }
    else
    {
        ASSERT_FALSE((bool)host.ipV6());
    }
}

void testHostAddress(
    const char* string4,
    const char* string6,
    boost::optional<in_addr_t> ipv4,
    boost::optional<in6_addr> ipv6)
{
    if (string4)
        testHostAddress(string4, ipv4, ipv6, string4);

    testHostAddress(string6, ipv4, ipv6, string6);

    if (ipv4)
    {
        in_addr addr = { *ipv4 };
        testHostAddress(addr, ipv4, ipv6, string4);
    }

    if (ipv6)
        testHostAddress(*ipv6, ipv4, ipv6, ipv4 ? string4 : string6);
}

TEST(HostAddressTest, Base)
{
    testHostAddress("0.0.0.0", "::", htonl(INADDR_ANY), in6addr_any);
    testHostAddress("127.0.0.1", "::1", htonl(INADDR_LOOPBACK), in6addr_loopback);

    const auto kIpV4a = HostAddress::ipV4from(QString("12.34.56.78"));
    const auto kIpV6a = HostAddress::ipV6from(QString("::ffff:c22:384e"));
    const auto kIpV6b = HostAddress::ipV6from(QString("2001:db8:0:2::1"));

    ASSERT_TRUE((bool)kIpV4a);
    ASSERT_TRUE((bool)kIpV6a);
    ASSERT_TRUE((bool)kIpV6b);

    testHostAddress("12.34.56.78", "::ffff:12.34.56.78", kIpV4a->s_addr, kIpV6a);
    testHostAddress(nullptr, "2001:db8:0:2::1", boost::none, kIpV6b);
}

void testSocketAddress(const char* init, const char* host, int port)
{
    const auto addr = SocketAddress(QString(init));
    ASSERT_EQ(addr.address.toString(), QString(host));
    ASSERT_EQ(addr.port, port);
    ASSERT_EQ(addr.toString(), QString(init));

    SocketAddress other(HostAddress(host), port);
    ASSERT_EQ(addr.toString(), other.toString());
    ASSERT_EQ(addr, other);

    QUrl url(QString("http://%1/path").arg(init));
    ASSERT_EQ(addr.address.toString(), url.host());
    ASSERT_EQ(addr.port, url.port(0));
}

TEST(SocketAddressTest, Base)
{
    testSocketAddress("ya.ru", "ya.ru", 0);
    testSocketAddress("ya.ru:80", "ya.ru", 80);

    testSocketAddress("12.34.56.78", "12.34.56.78", 0);
    testSocketAddress("12.34.56.78:123", "12.34.56.78", 123);

    testSocketAddress("[2001:db8:0:2::1]", "2001:db8:0:2::1", 0);
    testSocketAddress("[2001:db8:0:2::1]:23", "2001:db8:0:2::1", 23);

    testSocketAddress("[::ffff:12.34.56.78]", "::ffff:12.34.56.78", 0);
    testSocketAddress("[::ffff:12.34.56.78]:777", "::ffff:12.34.56.78", 777);
}

}   //test
}   //network
}   //nx
