/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include <common/common_globals.h>

#include "stun_server_connection.h"

#include "stun/stun_message_dispatcher.h"
#include "stun/custom_stun.h"

#include "stun_stream_socket_server.h"
#include <functional>

namespace nx {
namespace hpm {

StunServerConnection::StunServerConnection(
    StreamConnectionHolder<StunServerConnection>* socketServer,
    std::unique_ptr<AbstractCommunicatingSocket> sock )
:
    BaseType( socketServer, std::move( sock ) ),
    peer_address_( BaseType::getForeignAddress() )
{
}

void StunServerConnection::processMessage( stun::Message&& message )
{
    switch( message.header.messageClass )
    {
        case stun::MessageClass::request:
            switch( static_cast< stun::MethodType >( message.header.method ) )
            {
                case stun::MethodType::binding:
                    processGetIPAddressRequest( std::move(message) );
                    break;

                default:
                    processProprietaryRequest( std::move(message) );
                    break;
            }
            break;

        default:
            assert(false);  //not supported yet
    }
}

void StunServerConnection::processGetIPAddressRequest( stun::Message&& request )
{
    const auto systemAttr = request.getAttribute< attr::SystemName >();
    if( !systemAttr ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        "Attribute systemName is required" );
        return;
    }

    const auto systemName = systemAttr->value;
    if( !verifySystemName( systemName ) ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        String( "Wrong systemName: " ) + systemName );
        return;
    }

    const auto serverAttr = request.getAttribute< attr::ServerId >();
    if( !serverAttr ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        "Attribute serverId is required" );
        return;
    }

    const auto serverId = serverAttr->uuid();
    if( !verifyServerId( serverId ) ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        String( "Wrong serverId: " ) + serverId.toByteArray() );
        return;
    }

    const auto authAttr = request.getAttribute< attr::Authorization >();
    if( !authAttr ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        "Attribute authorization is required" );
        return;
    }
    if( !verifyAuthroization( authAttr->value ) ) {
        sendErrorReply( request.header.transactionID, stun::ErrorCode::badRequest,
                        String( "Authroization did not pass" ) );
        return;

    }

    sendSuccessReply( request.header.transactionID );
}

void StunServerConnection::sendSuccessReply( const stun::TransactionID& transaction_id ) {
    stun::Message message( stun::Header(
        stun::MessageClass::successResponse,
        stun::MethodType::binding, transaction_id ) );

    std::unique_ptr<stun::attr::XorMappedAddress> addr( new stun::attr::XorMappedAddress() );
    addr->family = stun::attr::XorMappedAddress::IPV4;
    SocketAddress peer_addr( peer_address_ );
    addr->address.ipv4 = peer_addr.address.ipv4(); // The endian is in host order 
    addr->port = peer_addr.port;
    message.addAttribute( std::move( addr ) );

    bool ret = sendMessage(std::move(message),
        std::bind(
        &StunServerConnection::onSendComplete,
        this,
        std::placeholders::_1));

    Q_ASSERT(ret);
}

void StunServerConnection::processProprietaryRequest( stun::Message&& request )
{
    if( !STUNMessageDispatcher::instance()->dispatchRequest( this, std::move(request) ) )
    {
        //TODO creating and sending error response
    }
}

void StunServerConnection::sendErrorReply( const stun::TransactionID& transaction_id,
                                           stun::ErrorCode::Type errorCode,
                                           nx::String description )
{
    // Currently for the reason phase, we just put empty string there
    stun::Message message( stun::Header(
        stun::MessageClass::errorResponse,
        Methods::listen, transaction_id ));
    message.addAttribute( std::make_unique< stun::attr::ErrorDescription >(
        errorCode, description ) );

    bool ret = sendMessage(std::move(message),
        std::bind(
        &StunServerConnection::onSendComplete,
        this,
        std::placeholders::_1));
    Q_ASSERT(ret);
}

bool StunServerConnection::verifySystemName( const String& name ) {
    Q_UNUSED(name);
    return true;
}

bool StunServerConnection::verifyServerId( const QnUuid& id ) {
    Q_UNUSED(id);
    return true;
}

bool StunServerConnection::verifyAuthroization( const String& auth ) {
    Q_UNUSED(auth);
    return true;
}

} // namespace hpm
} // namespace nx
