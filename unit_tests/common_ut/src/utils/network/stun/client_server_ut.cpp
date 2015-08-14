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

class StunClientServerTest
        : public ::testing::Test
{
protected:
    typedef MultiAddressServer< SocketServer > Server;

    StunClientServerTest()
        // qrand() is used to avoid possible address overlap
        : address( lit( "127.0.0.1"), 3345 + (qrand() % 100) )
        , client( address )
    {
    }

    std::unique_ptr< Server > makeServer()
    {
        auto server = std::make_unique< Server >(
                    std::list< SocketAddress >( 1, address ),
                    false, SocketFactory::nttAuto );

        EXPECT_TRUE( server->bind() );
        EXPECT_TRUE( server->listen() );

        return std::move(server);
    }

    const SocketAddress address;
    AsyncClient client;
};

TEST_F( StunClientServerTest, Connectivity )
{
    SyncQueue< SystemError::ErrorCode > connected;
    SyncQueue< SystemError::ErrorCode > disconnected;
    SyncQueue< Message > indication;

    // 1. try to connect with no server
    ASSERT_TRUE( client.openConnection(
        connected.pusher(), indication.pusher(), disconnected.pusher() ) );

    ASSERT_EQ( connected.pop(), SystemError::connectionRefused );
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indication.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );

    // 2. connect to normal server
    auto server = makeServer();

    ASSERT_TRUE( client.openConnection(
        connected.pusher(), indication.pusher(), disconnected.pusher() ) );

    ASSERT_EQ( connected.pop(), SystemError::noError );
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indication.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );

    // 3. remove server and see what's gonna happen
    server.reset();

    ASSERT_EQ( disconnected.pop(), SystemError::connectionReset );
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indication.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );
}

TEST_F( StunClientServerTest, RequestResponse )
{
    const auto server = makeServer();
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

TEST_F( StunClientServerTest, Indication )
{
    // TODO
}

} // namespace test
} // namespace hpm
} // namespase nx
