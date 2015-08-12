#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/stun/stun_connection.h>
#include <utils/network/stun/stun_message.h>
#include <utils/thread/sync_queue.h>
#include <stun/stun_server_connection.h>
#include <stun/stun_stream_socket_server.h>
#include <stun/stun_message_dispatcher.h>
#include <stun/custom_stun.h>
#include <listening_peer_pool.h>
#include <mediaserver_api.h>

namespace nx {
namespace hpm {
namespace test {

using namespace stun;

class StunSimpleTest : public testing::Test
{
protected:
    StunSimpleTest()
        : address( lit( "127.0.0.1"), 3345 + qrand() / 100 )
        , server( std::list< SocketAddress >( 1, address ), false, SocketFactory::nttAuto )
        , client( address )
    {
        EXPECT_TRUE( server.bind() );
        EXPECT_TRUE( server.listen() );

        SyncQueue< SystemError::ErrorCode > waiter;
        EXPECT_TRUE( client.openConnection( waiter.pusher() ) );
        EXPECT_EQ( waiter.pop(), SystemError::noError );
    }

    SocketAddress address;
    MultiAddressServer<StunStreamSocketServer> server;
    ClientConnection client;
};

TEST_F( StunSimpleTest, PublicAddress )
{
    {
        Message request( Header( MessageClass::request, MethodType::BINDING ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );

        const auto& response = result.second;
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, MethodType::BINDING );

        const auto addr = response.getAttribute< stun::attrs::XorMappedAddress >();
        ASSERT_NE( addr, nullptr );
        ASSERT_EQ( addr->family, stun::attrs::XorMappedAddress::IPV4 );
        ASSERT_EQ( addr->address.ipv4, address.address.ipv4() );
    }
    {
        Message request( Header( MessageClass::request, 0xFFF /* unknown */ ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );

        const auto& response = result.second;
        ASSERT_EQ( response.header.messageClass, MessageClass::errorResponse );
        ASSERT_EQ( response.header.method, 0xFFF );

        const auto error = response.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( error, nullptr );
        ASSERT_EQ( error->code, error::NOT_FOUND );
        ASSERT_EQ( error->reason, String( "Method is not supported" ).append( (char)0 ) );
    }
}

class MediaserverApiMock
        : public MediaserverApiIf
{
public:
    MOCK_METHOD2( ping, bool( const SocketAddress& address,
                              const QnUuid& expectedId ) );
};

class StunCustomTest : public StunSimpleTest
{
protected:
    StunCustomTest()
        : listeningPeerPool( &stunMessageDispatcher, &mediaserverApi )
    {
    }

    STUNMessageDispatcher stunMessageDispatcher;
    MediaserverApiMock mediaserverApi;
    ListeningPeerPool listeningPeerPool;
};

TEST_F( StunCustomTest, Ping )
{
    static const auto SYSTEM_ID = QnUuid::createUuid();
    static const auto SERVER_ID = QnUuid::createUuid();

    static const SocketAddress GOOD_ADDRESS( lit( "hello.world:123" ) );
    static const SocketAddress BAD_ADDRESS ( lit( "world.hello:321" ) );

    Message request( Header( MessageClass::request, methods::PING ) );
    request.newAttribute< hpm::attrs::SystemId >( SYSTEM_ID );
    request.newAttribute< hpm::attrs::ServerId >( SERVER_ID );
    request.newAttribute< hpm::attrs::Authorization >( "some_auth_data" );

    const std::list< SocketAddress > allEndpoints { GOOD_ADDRESS, BAD_ADDRESS };
    request.newAttribute< hpm::attrs::PublicEndpointList >( allEndpoints );

    // Suppose only one address is pingable
    EXPECT_CALL( mediaserverApi, ping( GOOD_ADDRESS, SERVER_ID ) )
        .Times( 1 ).WillOnce( ::testing::Return( true ) );
    EXPECT_CALL( mediaserverApi, ping( BAD_ADDRESS, SERVER_ID ) )
        .Times( 1 ).WillOnce( ::testing::Return( false ) );

    SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
    ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

    const auto result = waiter.pop();
    ASSERT_EQ( result.first, SystemError::noError );

    const auto& response = result.second;
    ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( response.header.method, methods::PING );

    const auto endpoints = response.getAttribute< attrs::PublicEndpointList >();
    ASSERT_NE( endpoints, nullptr );

    // Only good address is expected to be returned
    ASSERT_EQ( endpoints->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
}

} // namespace test
} // namespace hpm
} // namespase nx
