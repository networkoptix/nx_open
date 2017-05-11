
#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/std/cpp14.h>

#include <listening_peer_pool.h>
#include <peer_registrator.h>

#include "mediator_mocks.h"


namespace nx {
namespace hpm {
namespace test {

class ConnectTest : public testing::Test
{
protected:
    ConnectTest()
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();

        listeningPeerRegistrator = std::make_unique<PeerRegistrator>(
            settings,
            &cloud,
            &stunMessageDispatcher,
            &listeningPeerPool);
        server = std::make_unique<network::server::MultiAddressServer<stun::SocketServer>>(
            &stunMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled);

        EXPECT_TRUE(server->bind(std::vector<SocketAddress>{SocketAddress::anyAddress}));
        EXPECT_TRUE(server->listen());

        EXPECT_TRUE(server->endpoints().size());
        m_address = SocketAddress(HostAddress::localhost, server->endpoints().front().port);
        network::SocketGlobals::mediatorConnector().mockupAddress(m_address);
    }

    nx::stun::MessageDispatcher stunMessageDispatcher;

    CloudDataProviderMock cloud;
    conf::Settings settings;
    ListeningPeerPool listeningPeerPool;
    std::unique_ptr<PeerRegistrator> listeningPeerRegistrator;
    std::unique_ptr<network::server::MultiAddressServer<stun::SocketServer>> server;

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
    auto msClientGuard = makeScopeGuard([&msClient]() { msClient.pleaseStopSync(); });

    msClient.connect( address() );
    {
        stun::Message request( stun::Header( stun::MessageClass::request,
                                             stun::extension::methods::bind ) );
        request.newAttribute< stun::extension::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::extension::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::extension::attrs::PublicEndpointList >(
            std::list< SocketAddress >( 1, testHttpServer.serverAddress() ) );

        request.insertIntegrity( SYSTEM_ID, AUTH_KEY );
        cloud.expect_getSystem( SYSTEM_ID, AUTH_KEY );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, stun::Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass,
                   stun::MessageClass::successResponse );
    }

    const auto address = lit("http://%1.%2/test")
        .arg(QString::fromUtf8(SERVER_ID)).arg(QString::fromUtf8(SYSTEM_ID));

    nx_http::HttpClient client;
    if( nx::network::cloud::AddressResolver::kResolveOnMediator )
    {
        ASSERT_TRUE( client.doGet(address) );
        ASSERT_EQ( client.response()->statusLine.statusCode, nx_http::StatusCode::ok );
        ASSERT_EQ( client.fetchMessageBodyBuffer(), "test" );
    }
    else
    {
        ASSERT_FALSE( client.doGet(address) );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
