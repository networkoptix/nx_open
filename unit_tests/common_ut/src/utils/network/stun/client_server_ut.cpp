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
    static quint16 newPort()
    {
        static quint16 port = 3345;
        return ++port; // to avoid possible address overlap
    }

    StunClientServerTest()
        : address( lit( "127.0.0.1"), newPort() )
        , client( address )
    {
    }

    void startServer()
    {
        server = std::make_unique< SocketServer >( false );
        EXPECT_TRUE( server->bind( address ) );
        EXPECT_TRUE( server->listen() );
    }

    void stopServer()
    {
        server.reset();
    }

    const SocketAddress address;
    AsyncClient client;
    std::unique_ptr< SocketServer > server;
};

TEST_F( StunClientServerTest, Connectivity )
{
    SyncQueue< SystemError::ErrorCode > connected;
    SyncQueue< SystemError::ErrorCode > disconnected;
    SyncQueue< Message > indicated;

    // 1. try to connect with no server
    client.openConnection(
        connected.pusher(), indicated.pusher(), disconnected.pusher() );

    ASSERT_EQ( connected.pop(), SystemError::connectionRefused );
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indicated.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );

    // 2. connect to normal server
    startServer();

    client.openConnection(
        connected.pusher(), indicated.pusher(), disconnected.pusher() );

    ASSERT_EQ(SystemError::noError, connected.pop());
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indicated.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );

    // 3. remove server and see what's gonna happen
    stopServer();

    ASSERT_EQ(SystemError::connectionReset, disconnected.pop());
    EXPECT_TRUE( connected.isEmpty() );
    EXPECT_TRUE( indicated.isEmpty() );
    EXPECT_TRUE( disconnected.isEmpty() );
}

TEST_F( StunClientServerTest, RequestResponse )
{
    // try to sendRequest with no server
    {
        Message request( Header( MessageClass::request, MethodType::bindingMethod ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        client.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::connectionRefused );
    }

    startServer();
    {
        Message request( Header( MessageClass::request, MethodType::bindingMethod ) );

        SyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        client.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
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
        client.sendRequest( std::move( request ), waiter.pusher() );

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

} // namespace test
} // namespace hpm
} // namespase nx
