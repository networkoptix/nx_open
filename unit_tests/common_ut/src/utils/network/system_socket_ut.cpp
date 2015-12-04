#include <gtest/gtest.h>

#include <utils/common/cpp14.h>
#include <utils/thread/sync_queue.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>

#include <thread>

#define ERROR_TEXT SystemError::getLastOSErrorText().toStdString()

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

// Template multitype socket tests to ensure that every common_ut run checks
// TCP and UDT basic functionality

static const SocketAddress ADDRESS1( QLatin1String( "localhost:12345" ) );
static const SocketAddress ADDRESS2( QLatin1String( "localhost:12346" ) );
static const QByteArray MESSAGE( "Ping" );
static const size_t CLIENT_COUNT( 3 );

template< typename ServerSocket, typename ClientSocket>
static void socketSimpleSync()
{
    std::thread serverThread( []()
    {
        auto server = std::make_unique< ServerSocket >();
        ASSERT_TRUE( server->setNonBlockingMode( false ) );
        ASSERT_TRUE( server->setReuseAddrFlag( true ) );
        ASSERT_TRUE( server->bind( ADDRESS1 ) ) << ERROR_TEXT;
        ASSERT_TRUE( server->listen( CLIENT_COUNT ) ) << ERROR_TEXT;

        for( int i = CLIENT_COUNT; i > 0; --i )
        {
            static const int BUF_SIZE = 128;

            QByteArray buffer(BUF_SIZE, char(0));
            std::unique_ptr< AbstractStreamSocket > client(server->accept());
            ASSERT_TRUE(client->setNonBlockingMode(false));

            int bufDataSize = 0;
            for(;;)
            {
                const auto bytesRead = client->recv(
                    buffer.data()+ bufDataSize,
                    buffer.size()- bufDataSize);
                ASSERT_NE(-1, bytesRead);
                if (bytesRead == 0)
                    break;  //connection closed
                bufDataSize += bytesRead;
            }

            EXPECT_STREQ(MESSAGE.data(), buffer.data());
        }
    } );

    // give the server some time to start
    std::this_thread::sleep_for( std::chrono::microseconds( 500 ) );

    std::thread clientThread( []()
    {
        for( int i = CLIENT_COUNT; i > 0; --i )
        {
            auto client = std::make_unique< ClientSocket >( false );
            EXPECT_TRUE( client->connect( ADDRESS1, 500 ) );
            EXPECT_EQ( client->send( MESSAGE.data(), MESSAGE.size() + 1 ),
                       MESSAGE.size() + 1 );
        }
    } );

    serverThread.join();
    clientThread.join();
}

TEST( TcpSocket, SimpleSync )
{
    socketSimpleSync< TCPServerSocket, TCPSocket >();
}

TEST(SocketUdt, SimpleSync)
{
    socketSimpleSync< UdtStreamServerSocket, UdtStreamSocket >();
}

template< typename ServerSocket, typename ClientSocket>
static void socketSimpleAsync()
{
    nx::SyncQueue< SystemError::ErrorCode > serverResults;
    nx::SyncQueue< SystemError::ErrorCode > clientResults;

    auto server = std::make_unique< ServerSocket >();
    ASSERT_TRUE( server->setNonBlockingMode( true ) );
    ASSERT_TRUE( server->setReuseAddrFlag( true ) );
    ASSERT_TRUE( server->setRecvTimeout(5000) );
    ASSERT_TRUE( server->bind( ADDRESS2 ) ) << ERROR_TEXT;
    ASSERT_TRUE( server->listen( CLIENT_COUNT ) ) << ERROR_TEXT;

    QByteArray serverBuffer;
    serverBuffer.reserve( 128 );
    std::unique_ptr< AbstractStreamSocket > client;
    std::function< void( SystemError::ErrorCode, AbstractStreamSocket* ) > accept
            = [ & ]( SystemError::ErrorCode code, AbstractStreamSocket* socket )
    {
        serverResults.push( code );
        if( code != SystemError::noError )
            return;

        client.reset( socket );
        ASSERT_TRUE( client->setNonBlockingMode( true ) );
        client->readSomeAsync( &serverBuffer, [ & ]( SystemError::ErrorCode code,
                                               size_t size )
        {
            if( code == SystemError::noError ) {
                EXPECT_GT( size, 0 );
                EXPECT_STREQ( serverBuffer.data(), MESSAGE.data() );
                serverBuffer.resize( 0 );
            }

            client->terminateAsyncIO( true );
            server->acceptAsync( accept );
            serverResults.push( code );
        } );
    };

    server->acceptAsync( accept );

    auto testClient = std::make_unique< ClientSocket >( false );
    ASSERT_TRUE(testClient->setNonBlockingMode(true));
    ASSERT_TRUE(testClient->setSendTimeout(5000));

    //have to introduce wait here since subscribing to socket events (acceptAsync)
    //  takes some time and UDT ignores connections received before first accept call
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    QByteArray clientBuffer;
    clientBuffer.reserve( 128 );
    testClient->connectAsync( ADDRESS2, [ & ]( SystemError::ErrorCode code )
    {
        EXPECT_EQ( code, SystemError::noError );
        testClient->sendAsync( MESSAGE, [ & ]( SystemError::ErrorCode code,
                                               size_t size )
        {
            clientResults.push( code );
            if( code != SystemError::noError )
                return;

            EXPECT_EQ( code, SystemError::noError );
            EXPECT_EQ( size, MESSAGE.size() );
        } );
    } );

    ASSERT_EQ( serverResults.pop(), SystemError::noError ); // accept
    ASSERT_EQ( clientResults.pop(), SystemError::noError ); // send
    ASSERT_EQ( serverResults.pop(), SystemError::noError ); // recv

    testClient->terminateAsyncIO( true );
    server->terminateAsyncIO( true );
}

TEST( TcpSocket, SimpleAsync )
{
    socketSimpleAsync< TCPServerSocket, TCPSocket >();
}

TEST(SocketUdt, SimpleAsync)
{
    socketSimpleAsync< UdtStreamServerSocket, UdtStreamSocket >();
}
