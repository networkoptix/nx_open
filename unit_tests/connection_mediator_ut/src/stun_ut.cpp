#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/stun/async_client.h>
#include <utils/thread/sync_queue.h>
#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/stream_socket_server.h>
#include <utils/network/stun/message_dispatcher.h>
#include <utils/network/stun/cc/custom_stun.h>
#include <listening_peer_pool.h>
#include <mediaserver_api.h>

namespace nx {
namespace hpm {
namespace test {

using namespace stun;

class MediaserverApiMock
        : public MediaserverApiIf
{
public:
    MediaserverApiMock( stun::MessageDispatcher* dispatcher )
        : MediaserverApiIf( dispatcher ) {}

    MOCK_METHOD3( pingServer, void( const SocketAddress&, const String&,
                                    std::function< void( SocketAddress, bool ) > ) );
};

class StunCustomTest : public testing::Test
{
protected:
    StunCustomTest()
        : address( lit( "127.0.0.1"), 10001 + (qrand() % 50000) )
        , mediaserverApi( &stunMessageDispatcher )
        , listeningPeerPool( &stunMessageDispatcher )
        , server( false, SocketFactory::nttDisabled )
    {
        EXPECT_TRUE( server.bind( std::list< SocketAddress >( 1, address ) ) );
        EXPECT_TRUE( server.listen() );
    }

    SocketAddress address;
    MessageDispatcher stunMessageDispatcher;
    MediaserverApiMock mediaserverApi;
    ListeningPeerPool listeningPeerPool;
    MultiAddressServer<SocketServer> server;
};

static const auto SYSTEM_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID = QnUuid::createUuid().toSimpleString().toUtf8();

static const SocketAddress GOOD_ADDRESS( lit( "hello.world:123" ) );
static const SocketAddress BAD_ADDRESS ( lit( "world.hello:321" ) );

TEST_F( StunCustomTest, Ping )
{
    AsyncClient client( address );

    Message request( Header( MessageClass::request, cc::methods::ping ) );
    request.newAttribute< cc::attrs::SystemId >( SYSTEM_ID );
    request.newAttribute< cc::attrs::ServerId >( SERVER_ID );
    request.newAttribute< cc::attrs::Authorization >( "some_auth_data" );

    std::list< SocketAddress > allEndpoints;
    allEndpoints.push_back( GOOD_ADDRESS );
    allEndpoints.push_back( BAD_ADDRESS );
    request.newAttribute< cc::attrs::PublicEndpointList >( allEndpoints );

    // GOOD_ADDRESS shell be successfuly pinged
    EXPECT_CALL( mediaserverApi, pingServer( GOOD_ADDRESS, SERVER_ID, ::testing::_ ) )
        .Times( 1 ).WillOnce( ::testing::Invoke(
            []( const SocketAddress&, const String&,
                std::function< void( SocketAddress, bool ) > onPinged )
            { onPinged( GOOD_ADDRESS, true ); } ) );

    // while BAD_ADDRESS shell not pass
    EXPECT_CALL( mediaserverApi, pingServer( BAD_ADDRESS, SERVER_ID, ::testing::_  ) )
        .Times( 1 ).WillOnce( ::testing::Invoke(
            []( const SocketAddress&, const String&,
                std::function< void( SocketAddress, bool ) > onPinged )
            { onPinged( BAD_ADDRESS, false ); } ) );

    SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
    ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

    const auto result = waiter.pop();
    ASSERT_EQ( result.first, SystemError::noError );

    const auto& response = result.second;
    ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( response.header.method, cc::methods::ping );

    const auto eps = response.getAttribute< cc::attrs::PublicEndpointList >();
    ASSERT_NE( eps, nullptr );

    // only good address is expected to be returned
    ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
}

TEST_F( StunCustomTest, BindConnect )
{
    AsyncClient msClient( address );
    {
        Message request( Header( MessageClass::request, cc::methods::bind ) );
        request.newAttribute< cc::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< cc::attrs::ServerId >( SERVER_ID );
        request.newAttribute< cc::attrs::Authorization >( "some_auth_data" );
        request.newAttribute< cc::attrs::PublicEndpointList >(
                    std::list< SocketAddress >( 1, GOOD_ADDRESS ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( msClient.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );
    }

    AsyncClient connectClient( address );
    {
        Message request( Header( MessageClass::request, cc::methods::connect ) );
        request.newAttribute< cc::attrs::ClientId >( "SomeClient" );
        request.newAttribute< cc::attrs::HostName >( SYSTEM_ID );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( msClient.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );

        const auto eps = result.second.getAttribute< cc::attrs::PublicEndpointList >();
        ASSERT_NE( eps, nullptr );
        ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
    }
    {
        Message request( Header( MessageClass::request, cc::methods::connect ) );
        request.newAttribute< cc::attrs::ClientId >( "SomeClient" );
        request.newAttribute< cc::attrs::HostName >( "WrongHost" );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( msClient.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::errorResponse );

        const auto err = result.second.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( err, nullptr );
        ASSERT_EQ( err->code, stun::cc::error::notFound );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx

