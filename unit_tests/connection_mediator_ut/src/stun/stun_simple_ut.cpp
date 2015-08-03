#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/stun/stun_connection.h>
#include <utils/network/stun/stun_message.h>
#include <stun/stun_server_connection.h>
#include <stun/stun_stream_socket_server.h>
#include <stun/custom_stun.h>

#include <QMutex>
#include <QWaitCondition>

template< typename Result>
class AsyncWaiter
{
public:
    AsyncWaiter() : m_hasResult( false ) {}

    Result get()
    {
        QMutexLocker lock( &m_mutex );
        while( !m_hasResult )
            m_condition.wait( &m_mutex );

        m_hasResult = false;
        return std::move( m_result );
    }

    void set(Result result)
    {
        QMutexLocker lock( &m_mutex );
        m_result = std::move( result );
        m_hasResult = true;
        m_condition.wakeOne();
    }

private:
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_hasResult;
    Result m_result;
};

namespace nx {
namespace hpm {
namespace test {

using namespace stun;

static const SocketAddress ADDRESS( lit( "127.0.0.1"), 3345 );
static const std::list< SocketAddress > ADDRESS_LIST( 1, ADDRESS );

// FIXME: both tests can cot run in a single session because of our singletones
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
        AsyncWaiter< SystemError::ErrorCode > waiter;
        ASSERT_TRUE( client.openConnection(
             [ &waiter ]( SystemError::ErrorCode code )
             { waiter.set( code ); } ) );
        ASSERT_EQ( waiter.get(), SystemError::noError );
    }
    {
        Message request( Header( MessageClass::request, MethodType::BINDING ) );

        AsyncWaiter< Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ),
            [ &waiter ]( SystemError::ErrorCode code,
                         Message&& message )
        {
             ASSERT_EQ( code, SystemError::noError );
             waiter.set( std::move( message ) );
        } ) );

        Message response = waiter.get();
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, MethodType::BINDING );

        const auto addr = response.getAttribute< stun::attrs::XorMappedAddress >();
        ASSERT_NE( addr, nullptr );
        ASSERT_EQ( addr->family, stun::attrs::XorMappedAddress::IPV4 );
        ASSERT_EQ( addr->address.ipv4, ADDRESS.address.ipv4() );
    }
    {
        Message request( Header( MessageClass::request, 0xFFF /* unknown */ ) );

        AsyncWaiter< Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ),
            [ &waiter ]( SystemError::ErrorCode code,
                         Message&& message )
        {
             ASSERT_EQ( code, SystemError::noError );
             waiter.set( std::move( message ) );
        } ) );

        Message response = waiter.get();
        ASSERT_EQ( response.header.messageClass, MessageClass::errorResponse );
        ASSERT_EQ( response.header.method, 0xFFF );

        const auto error = response.getAttribute< stun::attrs::ErrorDescription >();
        ASSERT_NE( error, nullptr );
        ASSERT_EQ( error->code, error::NOT_FOUND );
        ASSERT_EQ( error->reason, String( "Method is not supported" ).append( (char)0 ) );
    }
}

TEST_F( StunSimpleTest, Custom )
{
    {
        AsyncWaiter< SystemError::ErrorCode > waiter;
        ASSERT_TRUE( client.openConnection(
             [ &waiter ]( SystemError::ErrorCode code )
             { waiter.set( code ); } ) );
        ASSERT_EQ( waiter.get(), SystemError::noError );
    }

    // TODO: create simple server for ping handling

    {
        Message request( Header( MessageClass::request, methods::PING ) );
        request.newAttribute< hpm::attrs::SystemId >( QnUuid::createUuid() );
        request.newAttribute< hpm::attrs::ServerId >( QnUuid::createUuid() );
        request.newAttribute< hpm::attrs::Authorization >( "some_auth_data" );
        request.newAttribute< hpm::attrs::PublicEndpointList >( ADDRESS_LIST );

        AsyncWaiter< Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ),
            [ &waiter ]( SystemError::ErrorCode code,
                         Message&& message )
        {
             ASSERT_EQ( code, SystemError::noError );
             waiter.set( std::move( message ) );
        } ) );

        Message response = waiter.get();
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, methods::PING );
    }
}

} // namespace test
} // namespace hpm
} // namespase nx
