/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_server_connection.h"

#include "stun/stun_message_dispatcher.h"
#include "stun/custom_stun.h"

#include "stun_stream_socket_server.h"


StunServerConnection::StunServerConnection(
    StunStreamSocketServer* socketServer,
    AbstractCommunicatingSocket* sock )
:
    BaseType( socketServer, sock )
{
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
    //TODO get remote host address from socket, create response and send with sendMessage( response )

    for( const nx_stun::Message::AttributesMap::value_type& attr: request.attributes )
    //std::for_each(request.attributes.begin(),request.attributes.end(),
    //    [this,request]( const std::unique_ptr<nx_stun::attr::Attribute>& attr )
    {
        if(attr.second->type != nx_stun::attr::AttributeType::unknownAttribute ) {
            sendErrorReply( nx_stun::ErrorCode::badRequest );
            return;
        }
        const nx_stun::attr::UnknownAttribute& unknown_attr = static_cast<const nx_stun::attr::UnknownAttribute&>(*attr.second);
        switch( unknown_attr.user_type ) {
        case nx_hpm::StunParameters::systemName:
            {
                nx_stun::attr::StringAttributeType system_name;
                if( !nx_stun::attr::parse( unknown_attr, &system_name ) )
                    return sendErrorReply( nx_stun::ErrorCode::badRequest );

                //TODO ...
                //Actually, we do not need custom attributes to be processed for this STUN method (Binding)

                //nx_hpm::StunParameters::SystemName system_name;
                //if( !system_name.parse(*unknown_attr) ) {
                //    sendErrorReply();
                //    return;
                //} else {
                //    // TODO
                //}
                break;
            }
        case nx_hpm::StunParameters::serverId:
            {
                // TODO
                break;
            }
        case nx_hpm::StunParameters::authorization:
            {
                // TODO
                break;
            }
        default: Q_ASSERT(0); return;
        }
    }
    //);
}

void StunServerConnection::processProprietaryRequest( nx_stun::Message&& request )
{
    if( !STUNMessageDispatcher::instance()->dispatchRequest( this, std::move(request) ) )
    {
        //TODO creating and sending error response
    }
}

void StunServerConnection::sendErrorReply( nx_stun::ErrorCode::Type /*errorCode*/ )
{
    //TODO
}
