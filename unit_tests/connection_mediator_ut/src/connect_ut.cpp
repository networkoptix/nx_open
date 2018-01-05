#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <common/common_globals.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/socket_global.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

#include <listening_peer_pool.h>
#include <peer_registrator.h>
#include <relay/relay_cluster_client.h>
#include <view.h>

#include "mediator_mocks.h"

using namespace nx::network;

namespace nx {
namespace hpm {
namespace test {

class ConnectTest:
    public testing::Test
{
protected:
    ConnectTest():
        m_listeningPeerPool(m_settings.listeningPeer())
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();

        m_relayClusterClient = std::make_unique<RelayClusterClient>(m_settings);

        m_listeningPeerRegistrator = std::make_unique<PeerRegistrator>(
            m_settings,
            &cloud,
            &m_listeningPeerPool,
            m_relayClusterClient.get());
        View::registerStunApiHandlers(m_listeningPeerRegistrator.get(), &m_stunMessageDispatcher);

        m_server = std::make_unique<stun::SocketServer>(
            &m_stunMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled);

        EXPECT_TRUE(m_server->bind(nx::network::SocketAddress::anyPrivateAddress));
        EXPECT_TRUE(m_server->listen());

        m_address = m_server->address();
        network::SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(m_address));
    }

    CloudDataProviderMock cloud;

    nx::network::SocketAddress address() const
    {
        return m_address;
    }

private:
    conf::Settings m_settings;
    ListeningPeerPool m_listeningPeerPool;
    std::unique_ptr<RelayClusterClient> m_relayClusterClient;
    std::unique_ptr<PeerRegistrator> m_listeningPeerRegistrator;

    nx::network::SocketAddress m_address;
    nx::network::stun::MessageDispatcher m_stunMessageDispatcher;
    std::unique_ptr<stun::SocketServer> m_server;
};

static const auto SYSTEM_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto AUTH_KEY = QnUuid::createUuid().toSimpleString().toUtf8();

TEST_F( ConnectTest, BindConnect )
{
    nx::network::http::TestHttpServer testHttpServer;
    {
        ASSERT_TRUE( testHttpServer.registerStaticProcessor( "/test", "test", "application/text" ) );
        ASSERT_TRUE( testHttpServer.bindAndListen() );
    }

    stun::AsyncClient msClient;
    auto msClientGuard = makeScopeGuard([&msClient]() { msClient.pleaseStopSync(); });

    msClient.connect(
        nx::network::url::Builder()
            .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address()));
    {
        stun::Message request( stun::Header( stun::MessageClass::request,
                                             stun::extension::methods::bind ) );
        request.newAttribute< stun::extension::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::extension::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::extension::attrs::PublicEndpointList >(
            std::list< nx::network::SocketAddress >( 1, testHttpServer.serverAddress() ) );

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

    nx::network::http::HttpClient client;
    ASSERT_FALSE( client.doGet(address) );
}

} // namespace test
} // namespace hpm
} // namespace nx
