#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <utils/thread/sync_queue.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/message_dispatcher.h>

namespace nx {
namespace stun {
namespace test {

class TestServer
    : public SocketServer
{
public:
    TestServer(const nx::stun::MessageDispatcher& dispatcher)
        : SocketServer(dispatcher, false) {}

    std::vector<std::shared_ptr<ServerConnection>> connections;

protected:
    virtual std::shared_ptr<ServerConnection> createConnection(
            std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        auto connection = SocketServer::createConnection(std::move(_socket));
        connections.push_back(connection);
        return connection;
    };
};

const AbstractAsyncClient::Timeouts CLIENT_TIMEOUTS = { 1000, 1000, 5000 };

class StunClientServerTest
        : public ::testing::Test
{
protected:
    static quint16 newPort()
    {
        static quint16 port = 3345;
        return ++port; // to avoid possible address overlap
    }

    StunClientServerTest()
        : address( lit( "127.0.0.1"), newPort() )
        , client( std::make_shared<AsyncClient>( CLIENT_TIMEOUTS ) )
    {
    }

    SystemError::ErrorCode sendTestRequestSync()
    {
        Message request( Header( MessageClass::request, MethodType::bindingMethod ) );
        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        client->sendRequest( std::move( request ), waiter.pusher() );
        return waiter.pop().first;
    }

    void startServer()
    {
        server = std::make_unique< TestServer >(dispatcher);
        EXPECT_TRUE( server->bind( address ) ) 
            << SystemError::getLastOSErrorText().toStdString();
        EXPECT_TRUE( server->listen() ) 
            << SystemError::getLastOSErrorText().toStdString();
    }

    SystemError::ErrorCode sendIndicationSync( int method )
    {
        SyncQueue< SystemError::ErrorCode > sendWaiter;
        const auto connection = server->connections.front();
        connection->sendMessage(
            Message( Header( MessageClass::indication, method ) ),
            sendWaiter.pusher() );

        return sendWaiter.pop();
    }

    const SocketAddress address;
    nx::stun::MessageDispatcher dispatcher;
    std::shared_ptr< AbstractAsyncClient > client;
    std::unique_ptr< TestServer > server;
};

TEST_F( StunClientServerTest, Connectivity )
{
    EXPECT_EQ( sendTestRequestSync(), SystemError::notConnected ); // no address

    client->connect( address );
    EXPECT_THAT( sendTestRequestSync(), testing::AnyOf(
        SystemError::connectionRefused, SystemError::connectionReset,
        SystemError::timedOut ) ); // no server to connect

    startServer();
    EXPECT_EQ( sendTestRequestSync(), SystemError::noError ); // ok
    EXPECT_EQ( server->connections.size(), 1 );

    server.reset();
    EXPECT_THAT( sendTestRequestSync(), testing::AnyOf(
        SystemError::connectionRefused, SystemError::connectionReset,
        SystemError::timedOut ) ); // no server to connect

    startServer();
    EXPECT_EQ( server->connections.size(), 0 );
    std::this_thread::sleep_for( std::chrono::milliseconds(
        CLIENT_TIMEOUTS.reconnect * 2 ) ); // automatic reconnect is expected
    EXPECT_EQ( server->connections.size(), 1 );
}

TEST_F( StunClientServerTest, RequestResponse )
{
    const auto t1 = std::chrono::steady_clock::now();

    startServer();
    client->connect(address);
    {
        Message request( Header( MessageClass::request, MethodType::bindingMethod ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        client->sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        const auto timePassed = 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t1);
        ASSERT_EQ( result.first, SystemError::noError );


        const auto& response = result.second;
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, MethodType::bindingMethod );

        const auto addr = response.getAttribute< stun::attrs::XorMappedAddress >();
        ASSERT_NE( addr, nullptr );
        ASSERT_EQ( addr->family, stun::attrs::XorMappedAddress::IPV4 );
        ASSERT_EQ( addr->address.ipv4, address.address.ipv4() );
    }
    {
        Message request( Header( MessageClass::request, 0xFFF /* unknown */ ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        client->sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );

        const auto& response = result.second;
        ASSERT_EQ( response.header.messageClass, MessageClass::errorResponse );
        ASSERT_EQ( response.header.method, 0xFFF );

        const auto error = response.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( error, nullptr );
        ASSERT_EQ( error->getCode(), 404 );
        ASSERT_EQ( error->getString(), String( "Method is not supported" ) );
    }
}

TEST_F( StunClientServerTest, Indications )
{
    startServer();

    SyncQueue< Message > recvWaiter;
    client->connect( address );
    client->setIndicationHandler( 0xAB, recvWaiter.pusher() );
    client->setIndicationHandler( 0xCD, recvWaiter.pusher() );

    EXPECT_EQ( sendTestRequestSync(), SystemError::noError );
    EXPECT_EQ( server->connections.size(), 1 );

    EXPECT_EQ( sendIndicationSync( 0xAB ), SystemError::noError );
    EXPECT_EQ( sendIndicationSync( 0xCD ), SystemError::noError );
    EXPECT_EQ( sendIndicationSync( 0xAD ), SystemError::noError );

    EXPECT_EQ( recvWaiter.pop().header.method, 0xAB );
    EXPECT_EQ( recvWaiter.pop().header.method, 0xCD );
    EXPECT_TRUE( recvWaiter.isEmpty() ); // 3rd indication is not subscribed
}

struct TestUser
    : public AsyncClientUser
{
    TestUser( std::shared_ptr<AbstractAsyncClient> client )
        : AsyncClientUser(std::move(client)) {}

    void request()
    {
        Message request( Header( MessageClass::request, MethodType::bindingMethod ) );
        sendRequest( std::move( request ), responses.pusher() );
    }

    SyncMultiQueue< SystemError::ErrorCode, Message > responses;
};

static const size_t USER_COUNT = 3;
static const size_t REQUEST_COUNT = 10;
static const size_t REQUEST_RATIO = 5;

TEST_F( StunClientServerTest, AsyncClientUser )
{
    startServer();
    client->connect( address );
    for( size_t uc = 0; uc < USER_COUNT; ++uc )
    {
        auto user = std::make_shared<TestUser>( client );
        for( size_t rc = 0; rc < REQUEST_COUNT * REQUEST_RATIO; ++rc )
            user->request();
        for( size_t rc = 0; rc < REQUEST_COUNT; ++rc ) // only part is waited
            EXPECT_EQ( user->responses.pop().first, SystemError::noError );
        user->pleaseStopSync();
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
