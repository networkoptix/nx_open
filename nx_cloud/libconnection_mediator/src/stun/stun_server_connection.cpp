/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_server_connection.h"

#include "stun/stun_message_dispatcher.h"
#include "stun/custom_stun.h"

#include "stun_stream_socket_server.h"
#include <functional>

StunServerConnection::StunServerConnection(
    StreamConnectionHolder<StunServerConnection>* socketServer,
    std::unique_ptr<AbstractCommunicatingSocket> sock )
:
    BaseType( socketServer, std::move( sock ) )
{
    peer_address_ = BaseType::getForeignAddress();
    // No way to detect error here
    bool ret = startReadingConnection();
    Q_ASSERT(ret);
}

void StunServerConnection::processMessage( nx_stun::Message&& message )
{
    switch( message.header.messageClass )
    {
        case nx_stun::MessageClass::request:
            switch( static_cast< nx_stun::MethodType >( message.header.method ) )
            {
                case nx_stun::MethodType::binding:
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

void StunServerConnection::processGetIPAddressRequest( nx_stun::Message&& request )
{
    bool system_name_ok = false;
    bool server_id_ok = false;
    bool authorization_ok = false;

    for( const nx_stun::Message::AttributesMap::value_type& attr: request.attributes )
    {
        if( attr.second->type() != nx_stun::attr::AttributeType::unknownAttribute ) {
            sendErrorReply( request.header.transactionID , nx_stun::ErrorCode::badRequest );
            return;
        }
        const nx_stun::attr::UnknownAttribute& unknown_attr = static_cast<const nx_stun::attr::UnknownAttribute&>(*attr.second);
        switch( unknown_attr.userType ) {
        case nx_hpm::StunParameters::systemName:
            {
                nx_stun::attr::StringAttributeType system_name;
                if( !nx_stun::attr::parse( unknown_attr, &system_name ) ) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
                    return;
                }
                system_name_ok = verifyServerName(system_name);
                if(!system_name_ok) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
                }
                break;
            }
        case nx_hpm::StunParameters::serverId:
            {
                nx_stun::attr::StringAttributeType server_id;
                if( !nx_stun::attr::parse( unknown_attr, &server_id ) ) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
                    return;
                }
                server_id_ok = verifyServerId(server_id);
                if(!server_id_ok) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
                    return;
                }
                break;
            }
        case nx_hpm::StunParameters::authorization:
            {
                nx_stun::attr::StringAttributeType authorization;
                if( !nx_stun::attr::parse( unknown_attr, &authorization ) ) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
                    return;
                }
                authorization_ok = verifyAuthroization(authorization);
                if(!authorization_ok) {
                    sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::unauthorized );
                    return;
                }
                break;
            }
        default: 
            {
                sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::unknownAttribute );
                return;
            }
        }
    }

    if( !system_name_ok || !server_id_ok || !authorization_ok ) {
        return sendErrorReply( request.header.transactionID, nx_stun::ErrorCode::badRequest );
    }
    sendSuccessReply( request.header.transactionID );
}

void StunServerConnection::sendSuccessReply( const nx_stun::TransactionID& transaction_id ) {
    nx_stun::Message message( nx_stun::Header(
        nx_stun::MessageClass::successResponse,
        static_cast<int>(nx_stun::MethodType::binding), transaction_id ) );

    std::unique_ptr<nx_stun::attr::XorMappedAddress> addr( new nx_stun::attr::XorMappedAddress() );
    addr->family = nx_stun::attr::XorMappedAddress::IPV4;
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

void StunServerConnection::processProprietaryRequest( nx_stun::Message&& request )
{
    if( !STUNMessageDispatcher::instance()->dispatchRequest( this, std::move(request) ) )
    {
        //TODO creating and sending error response
    }
}

void StunServerConnection::sendErrorReply( const nx_stun::TransactionID& transaction_id , nx_stun::ErrorCode::Type errorCode )
{
    // Currently for the reason phase, we just put empty string there
    nx_stun::Message message( nx_stun::Header(
        nx_stun::MessageClass::errorResponse,
        nx_hpm::StunMethods::listen, transaction_id ));
    message.addAttribute( std::make_unique< nx_stun::attr::ErrorDescription >( errorCode ) );

    bool ret = sendMessage(std::move(message),
        std::bind(
        &StunServerConnection::onSendComplete,
        this,
        std::placeholders::_1));
    Q_ASSERT(ret);
}

bool StunServerConnection::verifyAuthroization( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return true;
}

bool StunServerConnection::verifyServerId( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return true;
}

bool StunServerConnection::verifyServerName( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return true;
}

