
#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/async_client.h>
#include <utils/thread/sync_queue.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/common/cpp14.h>

#include <listening_peer_pool.h>
#include <peer_registrator.h>

#include "mediator_mocks.h"
#include <test_support/socket_globals_holder.h>


namespace nx {
namespace hpm {
namespace test {

class ConnectTest : public testing::Test
{
protected:
    ConnectTest()
    {
        SocketGlobalsHolder::instance()->reinitialize();

        m_address = SocketAddress(HostAddress::localhost, 10001 + (qrand() % 50000));
        listeningPeerRegistrator = std::make_unique<PeerRegistrator>(
            &cloud,
            &stunMessageDispatcher,
            &listeningPeerPool);
        server = std::make_unique<MultiAddressServer<stun::SocketServer>>(
            stunMessageDispatcher,
            false,
            SocketFactory::NatTraversalType::nttDisabled);

        EXPECT_TRUE(server->bind(std::list< SocketAddress >(1, m_address)));
        EXPECT_TRUE(server->listen());
        network::SocketGlobals::mediatorConnector().mockupAddress(m_address);
    }

    nx::stun::MessageDispatcher stunMessageDispatcher;

    CloudDataProviderMock cloud;
    ListeningPeerPool listeningPeerPool;
    std::unique_ptr<PeerRegistrator> listeningPeerRegistrator;
    std::unique_ptr<MultiAddressServer<stun::SocketServer>> server;

    SocketAddress address() const
    {
        return m_address;
    }

private:
    SocketAddress m_address;
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
    msClient.connect( address() );
    {
        stun::Message request( stun::Header( stun::MessageClass::request,
                                             stun::cc::methods::bind ) );
        request.newAttribute< stun::cc::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::cc::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::cc::attrs::PublicEndpointList >(
            std::list< SocketAddress >( 1, testHttpServer.serverAddress() ) );

        request.insertIntegrity( SYSTEM_ID, AUTH_KEY );
        cloud.expect_getSystem( SYSTEM_ID, AUTH_KEY );

        TestSyncMultiQueue< SystemError::ErrorCode, stun::Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass,
                   stun::MessageClass::successResponse );
    }

    nx_http::HttpClient client;
    {
        ASSERT_TRUE(client.doGet(lit("http://%1.%2/test")
            .arg(QString::fromUtf8(SERVER_ID)).arg(QString::fromUtf8(SYSTEM_ID))));
        ASSERT_EQ( client.response()->statusLine.statusCode, nx_http::StatusCode::ok );
        ASSERT_EQ( client.fetchMessageBodyBuffer(), "test" );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
