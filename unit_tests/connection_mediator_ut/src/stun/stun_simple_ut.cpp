#include <gtest/gtest.h>

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

namespace nx {
namespace hpm {
namespace test {

using namespace stun;

static const SocketAddress ADDRESS( lit( "127.0.0.1"), 3345 );
static const std::list< SocketAddress > ADDRESS_LIST( 1, ADDRESS );

class StunSimpleTest : public testing::Test
{
protected:
    StunSimpleTest()
        : server( ADDRESS_LIST, false, SocketFactory::nttAuto )
        , client( ADDRESS )
    {
        EXPECT_TRUE( server.bind() );
        EXPECT_TRUE( server.listen() );
    }

    MultiAddressServer<StunStreamSocketServer> server;
    StunClientConnection client;
};

TEST_F( StunSimpleTest, Base )
{
    {
        SyncQueue< SystemError::ErrorCode > waiter;
        ASSERT_TRUE( client.openConnection( waiter.pusher() ) );
        ASSERT_EQ( waiter.pop(), SystemError::noError );
    }
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
        ASSERT_EQ( addr->address.ipv4, ADDRESS.address.ipv4() );
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

class StunCustomTest : public StunSimpleTest
{
protected:
    StunCustomTest()
    {
        listeningPeerPool.registerRequestProcessors( stunMessageDispatcher );
    }

    ListeningPeerPool listeningPeerPool;
    STUNMessageDispatcher stunMessageDispatcher;
};

TEST_F( StunSimpleTest, Custom )
{
    {
        SyncQueue< SystemError::ErrorCode > waiter;
        ASSERT_TRUE( client.openConnection( waiter.pusher() ) );
        ASSERT_EQ( waiter.pop(), SystemError::noError );
    }

    //

    {
        Message request( Header( MessageClass::request, methods::PING ) );
        request.newAttribute< hpm::attrs::SystemId >( QnUuid::createUuid() );
        request.newAttribute< hpm::attrs::ServerId >( QnUuid::createUuid() );
        request.newAttribute< hpm::attrs::Authorization >( "some_auth_data" );
        request.newAttribute< hpm::attrs::PublicEndpointList >( ADDRESS_LIST );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ), waiter.pusher() ) );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );

        const auto& response = result.second;
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, methods::PING );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
