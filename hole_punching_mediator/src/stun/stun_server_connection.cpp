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
    AbstractCommunicatingSocket* sock )
:
    BaseType( socketServer, sock )
{
    // No way to detect error here
    bool ret = startReadingConnection();
    Q_ASSERT(ret);
}

void StunServerConnection::processMessage( nx_stun::Message&& message )
{
    switch( message.header.messageClass )
    {
        case nx_stun::MessageClass::request:
            switch( message.header.method )
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
        if(attr.second->type != nx_stun::attr::AttributeType::unknownAttribute ) {
            sendErrorReply( request.header.transactionID , nx_stun::ErrorCode::badRequest );
            return;
        }
        const nx_stun::attr::UnknownAttribute& unknown_attr = static_cast<const nx_stun::attr::UnknownAttribute&>(*attr.second);
        switch( unknown_attr.user_type ) {
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
    nx_stun::Message message;
    message.header.messageClass = nx_stun::MessageClass::successResponse;
    message.header.method = nx_hpm::StunMethods::listen;
    message.header.transactionID = transaction_id;
    std::unique_ptr<nx_stun::attr::XorMappedAddress> addr( new nx_stun::attr::XorMappedAddress() );
    addr->type = nx_stun::attr::AttributeType::xorMappedAddress;
    addr->family = nx_stun::attr::XorMappedAddress::IPV4;
    SocketAddress peer_addr( socket_->getPeerAddress() );
    addr->address.ipv4 = peer_addr.address.ipv4(); // The endian is in host order 
    addr->port = peer_addr.port;
    message.attributes.insert(
        std::make_pair(addr->type,
        std::unique_ptr<nx_stun::attr::Attribute>(addr.release())));
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
    nx_stun::Message message;
    message.header.messageClass = nx_stun::MessageClass::errorResponse;
    message.header.method = nx_hpm::StunMethods::listen;
    message.header.transactionID = transaction_id;
    std::unique_ptr<nx_stun::attr::ErrorDescription> error_code( new nx_stun::attr::ErrorDescription() );
    error_code->type = nx_stun::attr::AttributeType::errorCode;
    error_code->_class= errorCode / 100;
    error_code->code = errorCode;
    message.attributes.insert(std::make_pair(error_code->type,
        std::unique_ptr<nx_stun::attr::Attribute>(error_code.release())));
    bool ret = sendMessage(std::move(message),
        std::bind(
        &StunServerConnection::onSendComplete,
        this,
        std::placeholders::_1));
    Q_ASSERT(ret);
}

bool StunServerConnection::verifyAuthroization( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return false;
}

bool StunServerConnection::verifyServerId( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return false;
}

bool StunServerConnection::verifyServerName( const nx_stun::attr::StringAttributeType& name ) {
    Q_UNUSED(name);
    return false;
}

