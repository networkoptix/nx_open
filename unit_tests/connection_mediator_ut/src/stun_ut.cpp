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
    StunCustomTest()
        : address(lit("127.0.0.1"), 10001 + (qrand() % 50000))
        , mediaserverApi(&cloudData, &stunMessageDispatcher)
        , listeningPeerRegistrator(&cloudData, &stunMessageDispatcher, &listeningPeerPool)
        , server(
            &stunMessageDispatcher,
            false,
            SocketFactory::NatTraversalType::nttDisabled)
    {
        EXPECT_TRUE(server.bind(std::list< SocketAddress >(1, address)));
        EXPECT_TRUE(server.listen());
    }

    SocketAddress address;
    MessageDispatcher stunMessageDispatcher;
    CloudDataProviderMock cloudData;
    MediaserverApiMock mediaserverApi;
    ListeningPeerPool listeningPeerPool;
    PeerRegistrator listeningPeerRegistrator;
    MultiAddressServer<SocketServer> server;
};

static const auto SYSTEM_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto SERVER_ID = QnUuid::createUuid().toSimpleString().toUtf8();
static const auto AUTH_KEY = QnUuid::createUuid().toSimpleString().toUtf8();

static const SocketAddress GOOD_ADDRESS( lit( "hello.world:123" ) );
static const SocketAddress BAD_ADDRESS ( lit( "world.hello:321" ) );

TEST_F( StunCustomTest, Ping )
{
    AsyncClient client;
    client.connect( address );

    stun::Message request( Header( MessageClass::request, stun::cc::methods::ping ) );
    request.newAttribute< stun::cc::attrs::SystemId >( SYSTEM_ID );
    request.newAttribute< stun::cc::attrs::ServerId >( SERVER_ID );

    std::list< SocketAddress > allEndpoints;
    allEndpoints.push_back( GOOD_ADDRESS );
    allEndpoints.push_back( BAD_ADDRESS );
    request.newAttribute< stun::cc::attrs::PublicEndpointList >( allEndpoints );
    request.insertIntegrity(SYSTEM_ID, AUTH_KEY );

    cloudData.expect_getSystem( SYSTEM_ID, AUTH_KEY );
    mediaserverApi.expect_pingServer( GOOD_ADDRESS, SERVER_ID, true );
    mediaserverApi.expect_pingServer( BAD_ADDRESS, SERVER_ID, false );

    TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
    client.sendRequest( std::move( request ), waiter.pusher() );

    const auto result = waiter.pop();
    ASSERT_EQ( result.first, SystemError::noError );

    const auto& response = result.second;
    ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( response.header.method, stun::cc::methods::ping );

    const auto eps = response.getAttribute< stun::cc::attrs::PublicEndpointList >();
    ASSERT_NE( eps, nullptr );

    // only good address is expected to be returned
    ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
}

TEST_F( StunCustomTest, BindResolve )
{
    AsyncClient msClient;
    msClient.connect( address );
    {
        stun::Message request( Header( MessageClass::request, stun::cc::methods::bind ) );
        request.newAttribute< stun::cc::attrs::SystemId >( SYSTEM_ID );
        request.newAttribute< stun::cc::attrs::ServerId >( SERVER_ID );
        request.newAttribute< stun::cc::attrs::PublicEndpointList >(
                    std::list< SocketAddress >( 1, GOOD_ADDRESS ) );

        request.insertIntegrity(SYSTEM_ID, AUTH_KEY );
        cloudData.expect_getSystem( SYSTEM_ID, AUTH_KEY );

        TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );
    }

    AsyncClient connectClient;
    connectClient.connect( address );
    {
        stun::Message request( Header(
            MessageClass::request, stun::cc::methods::resolveDomain ) );
        request.newAttribute< stun::cc::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::cc::attrs::HostName >( SYSTEM_ID );

        TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );

        const auto attr = result.second.getAttribute< stun::cc::attrs::HostNameList >();
        ASSERT_NE( attr, nullptr );
        ASSERT_EQ( attr->get(), std::vector< String >(
            1, SERVER_ID + "." + SYSTEM_ID ) );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::cc::methods::resolvePeer ) );
        request.newAttribute< stun::cc::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::cc::attrs::HostName >(
            SERVER_ID + "." + SYSTEM_ID );

        TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::successResponse );

        const auto eps = result.second.getAttribute< stun::cc::attrs::PublicEndpointList >();
        ASSERT_NE( eps, nullptr );
        ASSERT_EQ( eps->get(), std::list< SocketAddress >( 1, GOOD_ADDRESS ) );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::cc::methods::resolveDomain) );
        request.newAttribute< stun::cc::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::cc::attrs::HostName >( "WrongDomain" );

        TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::errorResponse );

        const auto err = result.second.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( err, nullptr );
        ASSERT_EQ( err->getCode(), stun::cc::error::notFound );
    }
    {
        stun::Message request( Header(
            MessageClass::request, stun::cc::methods::resolvePeer) );
        request.newAttribute< stun::cc::attrs::PeerId >( "SomeClient" );
        request.newAttribute< stun::cc::attrs::HostName >( "WrongHost" );

        TestSyncMultiQueue< SystemError::ErrorCode, Message > waiter;
        msClient.sendRequest( std::move( request ), waiter.pusher() );

        const auto result = waiter.pop();
        ASSERT_EQ( result.first, SystemError::noError );
        ASSERT_EQ( result.second.header.messageClass, MessageClass::errorResponse );

        const auto err = result.second.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( err, nullptr );
        ASSERT_EQ( err->getCode(), stun::cc::error::notFound );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
