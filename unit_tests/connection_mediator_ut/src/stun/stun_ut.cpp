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

namespace nx_stun {

TEST( Stun, Simple )
{
    SocketAddress localhost( lit( "127.0.0.1"), 3345 );

    std::list<SocketAddress> addrList;
    addrList.push_back( localhost );
    MultiAddressServer<StunStreamSocketServer> server(
                addrList, true, SocketFactory::nttAuto );
    ASSERT_TRUE( server.bind() );
    ASSERT_TRUE( server.listen() );

    StunClientConnection client( localhost );
    {
        AsyncWaiter< SystemError::ErrorCode > waiter;
        ASSERT_TRUE( client.openConnection(
             [ &waiter ]( SystemError::ErrorCode code )
             { waiter.set( code ); } ) );
        ASSERT_EQ( waiter.get(), SystemError::noError );
    }
    {
        Message request( Header( MessageClass::request,
                                 static_cast< int >( MethodType::binding ),
                                 TransactionID::generateNew() ) );
        request.addAttribute( std::make_unique<nx_stun::attr::UnknownAttribute>(
                nx_hpm::StunParameters::systemName, QByteArray( "system1" ) ) );
        request.addAttribute( std::make_unique<nx_stun::attr::UnknownAttribute>(
                nx_hpm::StunParameters::serverId, QByteArray( "{uuid1}" ) ) );
        request.addAttribute( std::make_unique<nx_stun::attr::UnknownAttribute>(
                nx_hpm::StunParameters::authorization, QByteArray( "auth" ) ) );

        AsyncWaiter< Message > waiter;
        ASSERT_TRUE( client.sendRequest( std::move( request ),
            [ &waiter ]( SystemError::ErrorCode code,
                         nx_stun::Message&& message )
        {
             ASSERT_EQ( code, SystemError::noError );
             waiter.set( std::move( message ) );
        } ) );

        nx_stun::Message response = waiter.get();
        ASSERT_EQ( response.header.messageClass, MessageClass::successResponse );
        ASSERT_EQ( response.header.method, static_cast< int >( MethodType::binding ) );

        const auto addr = response.getAttribute<attr::XorMappedAddress>();
        ASSERT_NE( addr, nullptr );
        ASSERT_EQ( addr->family, nx_stun::attr::XorMappedAddress::IPV4 );
        ASSERT_EQ( addr->address.ipv4, localhost.address.ipv4() );
    }
}

} // namespace nx_stun
