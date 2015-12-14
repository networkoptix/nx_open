#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/async_client.h>
#include <utils/thread/sync_queue.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>

#include "mediator_mocks.h"

namespace nx {
namespace hpm {
namespace test {

class ConnectTest : public testing::Test
{
protected:
    ConnectTest()
        : address( lit( "127.0.0.1"), 10001 + (qrand() % 50000) )
        , listeningPeerPool( &cloud, &stunMessageDispatcher )
        , server( false, SocketFactory::NatTraversalType::nttDisabled )
    {
        EXPECT_TRUE( server.bind( std::list< SocketAddress >( 1, address ) ) );
        EXPECT_TRUE( server.listen() );
        SocketGlobals::mediatorConnector().mockupAddress( address );
    }

    const SocketAddress address;
    stun::MessageDispatcher stunMessageDispatcher;

    CloudDataProviderMock cloud;
    ListeningPeerPool listeningPeerPool;
    MultiAddressServer< stun::SocketServer > server;
};

static const auto SYSTEM_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto AUTH_KEY = QnUuid::createUuid().toSimpleString().toUtf8();

TEST_F( ConnectTest, BindConnect )
{
    TestHttpServer testHttpServer;
    {
        ASSERT_TRUE( testHttpServer.registerStaticProcessor( "/test", "test", "application/text" ) );
        ASSERT_TRUE( testHttpServer.bindAndListen() );
    }

    stun::AsyncClient msClient;
    msClient.connect( address );
    {
        stun::Message request( stun::Header( stun::MessageClass::request,
                                             stun::cc::methods::bind ) );
        request.newAttribute< stun::cc::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::cc::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::cc::attrs::PublicEndpointList >(
            std::list< SocketAddress >( 1, testHttpServer.serverAddress() ) );

        request.insertIntegrity( SYSTEM_ID, AUTH_KEY );
        cloud.expect_getSystem( SYSTEM_ID, AUTH_KEY );

        SyncMultiQueue< SystemError::ErrorCode, stun::Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass,
                   stun::MessageClass::successResponse );
    }

    nx_http::HttpClient client;
    {
        ASSERT_TRUE( client.doGet( lit("http://%1/test")
                                   .arg( QString::fromUtf8( SYSTEM_ID ) ) ) );
        ASSERT_EQ( client.response()->statusLine.statusCode, nx_http::StatusCode::ok );
        ASSERT_EQ( client.fetchMessageBodyBuffer(), "test" );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
