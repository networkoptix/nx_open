#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/sync_queue.h>

#include <common/common_globals.h>
#include <nx/utils/scope_guard.h>

#include <listening_peer_pool.h>
#include <peer_registrator.h>
#include <mediaserver_api.h>

#include "mediator_mocks.h"

namespace nx {
namespace hpm {
namespace test {

using namespace stun;

class StunCustomTest : public testing::Test
{
protected:
    StunCustomTest():
        mediaserverApi(&cloudData, &stunMessageDispatcher),
        listeningPeerRegistrator(settings, &cloudData, &stunMessageDispatcher, &listeningPeerPool),
        server(
            &stunMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled)
    {
        EXPECT_TRUE(server.bind(std::vector<SocketAddress>{SocketAddress::anyAddress}));
        EXPECT_TRUE(server.listen());

        EXPECT_TRUE(server.endpoints().size());
        address = SocketAddress(HostAddress::localhost, server.endpoints().front().port);
    }

    ~StunCustomTest()
    {
        server.pleaseStopSync();
    }

    SocketAddress address;
    MessageDispatcher stunMessageDispatcher;
    CloudDataProviderMock cloudData;
    MediaserverApiMock mediaserverApi;
    conf::Settings settings;
    ListeningPeerPool listeningPeerPool;
    PeerRegistrator listeningPeerRegistrator;
    network::server::MultiAddressServer<SocketServer> server;
};

static const auto SYSTEM_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID2 = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto AUTH_KEY = QnUuid::createUuid().toSimpleString().toUtf8();

static const SocketAddress GOOD_ADDRESS( lit( "hello.world:123" ) );
static const SocketAddress BAD_ADDRESS ( lit( "world.hello:321" ) );

TEST_F( StunCustomTest, Ping )
{
    AsyncClient client;
    auto clientGuard = makeScopeGuard([&client]() { client.pleaseStopSync(); });

    client.connect( address );

    stun::Message request( Header( MessageClass::request, stun::extension::methods::ping ) );
    request.newAttribute< stun::extension::attrs::SystemId >( SYSTEM_ID );
    request.newAttribute< stun::extension::attrs::ServerId >( SERVER_ID );

    std::list< SocketAddress > allEndpoints;
    allEndpoints.push_back( GOOD_ADDRESS );
    allEndpoints.push_back( BAD_ADDRESS );
    request.newAttribute< stun::extension::attrs::PublicEndpointList >( allEndpoints );
    request.insertIntegrity(SYSTEM_ID, AUTH_KEY );

    cloudData.expect_getSystem( SYSTEM_ID, AUTH_KEY );
    mediaserverApi.expect_pingServer( GOOD_ADDRESS, SERVER_ID, true );
    mediaserverApi.expect_pingServer( BAD_ADDRESS, SERVER_ID, false );

    nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
    client.sendRequest( std::move( request ), waiter.pusher() );

    const auto result = waiter.pop();
    ASSERT_EQ( result.first, SystemError::noError );

    const auto& response = result.second;
    ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( response.header.method, stun::extension::methods::ping );

    const auto eps = response.getAttribute< stun::extension::attrs::PublicEndpointList >();
    ASSERT_NE( eps, nullptr );

    // only good address is expected to be returned
    ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
}

TEST_F( StunCustomTest, BindResolve )
{
    AsyncClient msClient;
    auto msClientGuard = makeScopeGuard([&msClient]() { msClient.pleaseStopSync(); });

    msClient.connect( address );
    {
        stun::Message request( Header( MessageClass::request, stun::extension::methods::bind ) );
        request.newAttribute< stun::extension::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::extension::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::extension::attrs::PublicEndpointList >(
                    std::list< SocketAddress >( 1, GOOD_ADDRESS ) );

        request.insertIntegrity(SYSTEM_ID, AUTH_KEY );
        cloudData.expect_getSystem( SYSTEM_ID, AUTH_KEY );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );
    }

    AsyncClient connectClient;
    auto connectClientGuard = makeScopeGuard([&connectClient]() { connectClient.pleaseStopSync(); });

    connectClient.connect( address );
    {
        stun::Message request( Header(
            MessageClass::request, stun::extension::methods::resolveDomain ) );
        request.newAttribute< stun::extension::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::extension::attrs::HostName >( SYSTEM_ID );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );

        const auto attr = result.second.getAttribute< stun::extension::attrs::HostNameList >();
        ASSERT_NE( attr, nullptr );
        ASSERT_EQ( attr->get(), std::vector< String >(
            1, SERVER_ID + "." + SYSTEM_ID ) );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::extension::methods::resolvePeer ) );
        request.newAttribute< stun::extension::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::extension::attrs::HostName >(
            SERVER_ID + "." + SYSTEM_ID );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );

        const auto eps = result.second.getAttribute< stun::extension::attrs::PublicEndpointList >();
        ASSERT_NE( eps, nullptr );
        ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::extension::methods::resolveDomain) );
        request.newAttribute< stun::extension::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::extension::attrs::HostName >( "WrongDomain" );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::errorResponse );

        const auto err = result.second.getAttribute< stun::attrs::ErrorCode >();
        ASSERT_NE( err, nullptr );
        ASSERT_EQ( err->getCode(), stun::extension::error::notFound );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::extension::methods::resolvePeer) );
        request.newAttribute< stun::extension::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::extension::attrs::HostName >( "WrongHost" );

        nx::utils::TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::errorResponse );

        const auto err = result.second.getAttribute< stun::attrs::ErrorCode >();
        ASSERT_NE( err, nullptr );
        ASSERT_EQ( err->getCode(), stun::extension::error::notFound );
    }
}

typedef std::unique_ptr<utils::TestSyncMultiQueue<String, SocketAddress>> IndicatinonQueue;

IndicatinonQueue listenForClientBind(
    AsyncClient* client, const String& serverId, const conf::Settings& settings)
{
    auto queue = std::make_unique<utils::TestSyncMultiQueue<String, SocketAddress>>();
    client->setIndicationHandler(
        stun::extension::indications::connectionRequested,
        [queue = queue.get(), &settings](stun::Message message)
        {
            api::ConnectionRequestedEvent event;
            EXPECT_TRUE(event.parseAttributes(message));
            EXPECT_EQ(event.tcpReverseEndpointList.size(), 1U);
            EXPECT_EQ(event.params, settings.connectionParameters());
            EXPECT_EQ(event.isPersistent, true);
            queue->push(
                std::move(event.originatingPeerID),
                std::move(event.tcpReverseEndpointList.front()));
        });

    api::ListenRequest request;
    request.systemId = SYSTEM_ID;
    request.serverId = serverId;

    nx::stun::Message message(stun::Header(stun::MessageClass::request, request.kMethod));
    request.serialize(&message);
    message.insertIntegrity(SYSTEM_ID, AUTH_KEY);

    nx::utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> waiter;
    client->sendRequest(std::move(message), waiter.pusher());

    std::pair<SystemError::ErrorCode, Message> result = waiter.pop();
    EXPECT_EQ(result.first, SystemError::noError);
    EXPECT_EQ(result.second.header.messageClass, MessageClass::successResponse);
    return queue;
}

void expectIndicationForEach(
    std::vector<IndicatinonQueue::element_type*> msIndicationsList,
    const String& peerId, const SocketAddress& address, bool exactlyOne = true)
{
    for (auto& msIndications: msIndicationsList)
        ASSERT_EQ(msIndications->pop(), std::make_pair(peerId, address));

    if (exactlyOne)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (auto& msIndications: msIndicationsList)
            ASSERT_TRUE(msIndications->isEmpty());
    }
}

void bindClientSync(AsyncClient* client, const String& id, const SocketAddress& address)
{
    api::ClientBindRequest request;
    request.originatingPeerID = id;
    request.tcpReverseEndpoints.push_back(address);

    nx::stun::Message message(stun::Header(stun::MessageClass::request, request.kMethod));
    request.serialize(&message);

    nx::utils::TestSyncMultiQueue<SystemError::ErrorCode, Message> waiter;
    client->sendRequest(std::move(message), waiter.pusher());

    const auto result = waiter.pop();
    ASSERT_EQ(result.first, SystemError::noError);
    ASSERT_EQ(result.second.header.messageClass, MessageClass::successResponse);
}

TEST_F(StunCustomTest, ClientBind)
{
    typedef std::chrono::seconds s;
    auto& params = const_cast<conf::ConnectionParameters&>(settings.connectionParameters());
    params.tcpReverseRetryPolicy = nx::network::RetryPolicy(3, s(1), 2, s(7));
    params.tcpReverseHttpTimeouts = nx_http::AsyncHttpClient::Timeouts(s(1), s(2), s(3));
    cloudData.expect_getSystem(SYSTEM_ID, AUTH_KEY, 3);

    AsyncClient msClient;
    auto msClientGuard = makeScopeGuard([&msClient]() { msClient.pleaseStopSync(); });

    msClient.connect(address);
    const auto msIndications = listenForClientBind(&msClient, SERVER_ID, settings);

    AsyncClient bindClient;
    auto bindClientGuard = makeScopeGuard([&bindClient]() { bindClient.pleaseStopSync(); });

    bindClient.connect(address);
    bindClientSync(&bindClient, "VmsGateway", GOOD_ADDRESS);

    AsyncClient msClient2;
    auto msClient2Guard = makeScopeGuard([&msClient2]() { msClient2.pleaseStopSync(); });

    msClient2.connect(address);
    const auto msIndications2 = listenForClientBind(&msClient2, SERVER_ID2, settings);

    // Both servers get just one indication:
    expectIndicationForEach({msIndications.get(), msIndications2.get()}, "VmsGateway", GOOD_ADDRESS);

    bindClientSync(&bindClient, "VmsGateway", BAD_ADDRESS);
    expectIndicationForEach({msIndications.get(), msIndications2.get()}, "VmsGateway", BAD_ADDRESS);

    auto bindClient2 = std::make_unique<AsyncClient>();
    bindClient2->connect(address);
    bindClientSync(bindClient2.get(), "VmsGateway2", GOOD_ADDRESS);

    // Both servers get just one indication:
    expectIndicationForEach({msIndications.get(), msIndications2.get()}, "VmsGateway2", GOOD_ADDRESS);

    // Disconnect one gateway:
    bindClient2->pleaseStopSync();
    bindClient2.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The next server gets only one indication:
    AsyncClient msClient3;
    auto msClient3Guard = makeScopeGuard([&msClient3]() { msClient3.pleaseStopSync(); });

    msClient3.connect(address);
    const auto msIndications3 = listenForClientBind(&msClient3, SERVER_ID, settings);
    expectIndicationForEach({msIndications3.get()}, "VmsGateway", BAD_ADDRESS);
}

} // namespace test
} // namespace hpm
} // namespace nx
