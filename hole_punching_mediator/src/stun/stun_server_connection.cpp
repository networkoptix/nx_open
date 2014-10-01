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
    std::for_each(request.attributes.begin(),request.attributes.end(),
        [this,request]( const std::unique_ptr<nx_stun::attr::Attribute>& attr ) {
        if(attr->type != nx_stun::attr::AttributeType::unknownAttribute ) {
            sendErrorReply();
            return;
        }
        nx_stun::attr::UnknownAttribute* unknown_attr = static_cast<nx_stun::attr::UnknownAttribute*>(attr.get());
        switch( unknown_attr->user_type ) {
        case nx_hpm::StunParameters::systemName:
            {
                nx_hpm::StunParameters::SystemName system_name;
                if( !system_name.parse(*unknown_attr) ) {
                    sendErrorReply();
                    return;
                } else {
                    // TODO
                }
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
    });
}

void StunServerConnection::processProprietaryRequest( nx_stun::Message&& request )
{
    if( !STUNMessageDispatcher::instance()->dispatchRequest( std::static_pointer_cast<StunServerConnection>(shared_from_this()), std::move(request) ) )
    {
        //TODO creating and sending error response
    }
}
