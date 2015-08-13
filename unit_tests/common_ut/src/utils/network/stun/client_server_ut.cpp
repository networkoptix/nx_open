#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/thread/sync_queue.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/stun/async_client.h>
#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/stream_socket_server.h>
#include <utils/network/stun/message_dispatcher.h>

namespace nx {
namespace stun {
namespace test {

TEST( StunClientServer, RequestResponse )
{
    const SocketAddress address( lit( "127.0.0.1"), 3345 + qrand() / 100 );
    MultiAddressServer<SocketServer> server(
                std::list< SocketAddress >( 1, address ),
                false, SocketFactory::nttAuto );

    EXPECT_TRUE( server.bind() );
    EXPECT_TRUE( server.listen() );

    AsyncClient client( address );
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
        ASSERT_EQ( error->code, 404 );
        ASSERT_EQ( error->reason, String( "Method is not supported" ).append( (char)0 ) );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
