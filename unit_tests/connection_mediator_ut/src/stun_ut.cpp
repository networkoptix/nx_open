#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/stun/async_client.h>
#include <utils/thread/sync_queue.h>
#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/stream_socket_server.h>
#include <utils/network/stun/message_dispatcher.h>
#include <stun/custom_stun.h>
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

    MOCK_METHOD2( pingServer, bool( const SocketAddress&, const QnUuid& ) );
};

class StunCustomTest : public testing::Test
{
protected:
    StunCustomTest()
        : address( lit( "127.0.0.1"), 3345 + qrand() / 100 )
        , server( std::list< SocketAddress >( 1, address ), false, SocketFactory::nttAuto )
        , mediaserverApi( &stunMessageDispatcher )
        , listeningPeerPool( &stunMessageDispatcher )
        , client( address )
    {
        EXPECT_TRUE( server.bind() );
        EXPECT_TRUE( server.listen() );
    }

    SocketAddress address;
    MultiAddressServer<SocketServer> server;
    MessageDispatcher stunMessageDispatcher;
    MediaserverApiMock mediaserverApi;
    ListeningPeerPool listeningPeerPool;

    AsyncClient client;
};

TEST_F( StunCustomTest, Ping )
{
    static const auto SYSTEM_ID = QnUuid::createUuid();
    static const auto SERVER_ID = QnUuid::createUuid();

    static const SocketAddress GOOD_ADDRESS( lit( "hello.world:123" ) );
    static const SocketAddress BAD_ADDRESS ( lit( "world.hello:321" ) );

    Message request( Header( MessageClass::request, methods::ping ) );
    request.newAttribute< hpm::attrs::SystemId >( SYSTEM_ID );
    request.newAttribute< hpm::attrs::ServerId >( SERVER_ID );
    request.newAttribute< hpm::attrs::Authorization >( "some_auth_data" );

    const std::list< SocketAddress > allEndpoints { GOOD_ADDRESS, BAD_ADDRESS };
    request.newAttribute< hpm::attrs::PublicEndpointList >( allEndpoints );

    // Suppose only one address is pingable
    EXPECT_CALL( mediaserverApi, pingServer( GOOD_ADDRESS, SERVER_ID ) )
        .Times( 1 ).WillOnce( ::testing::Return( true ) );
    EXPECT_CALL( mediaserverApi, pingServer( BAD_ADDRESS, SERVER_ID ) )
        .Times( 1 ).WillOnce( ::testing::Return( false ) );

    SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
    ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

    const auto result = waiter.pop();
    ASSERT_EQ( result.first, SystemError::noError );

    const auto& response = result.second;
    ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( response.header.method, methods::ping );

    const auto endpoints = response.getAttribute< attrs::PublicEndpointList >();
    ASSERT_NE( endpoints, nullptr );

    // Only good address is expected to be returned
    ASSERT_EQ( endpoints->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
}

} // namespace test
} // namespace hpm
} // namespase nx
