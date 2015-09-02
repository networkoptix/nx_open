/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "server_connection.h"

#include "message_dispatcher.h"

#include <common/common_globals.h>

namespace nx {
namespace stun {

ServerConnection::ServerConnection(
    StreamConnectionHolder<ServerConnection>* socketServer,
    std::unique_ptr<AbstractCommunicatingSocket> sock )
:
    BaseType( socketServer, std::move( sock ) ),
    peer_address_( BaseType::getForeignAddress() )
{
}

void ServerConnection::processMessage( Message message )
{
    switch( message.header.messageClass )
    {
        case MessageClass::request:
            switch( message.header.method )
            {
                case bindingMethod:
                    processBindingRequest( std::move( message ) );
                    break;

                default:
                    processCustomRequest( std::move( message ) );
                    break;
            }
            break;

        default:
            Q_ASSERT( false );  //not supported yet
    }
}

void ServerConnection::processBindingRequest( Message message )
{
    Message response( stun::Header(
        MessageClass::successResponse,
        bindingMethod, std::move( message.header.transactionId ) ) );

    SocketAddress peer( peer_address_ );
    response.newAttribute< stun::attrs::XorMappedAddress >(
                peer.port, peer.address.ipv4() );

    sendMessage( std::move( response ) );
}

void ServerConnection::processCustomRequest( Message message )
{
    if( auto disp = MessageDispatcher::instance() )
        if( disp->dispatchRequest( this, std::move(message) ) )
            return;

    stun::Message response( stun::Header(
        stun::MessageClass::errorResponse,
        message.header.method,
        std::move( message.header.transactionId ) ) );

    // TODO: verify with RFC
    response.newAttribute< stun::attrs::ErrorDescription >(
        404, "Method is not supported" );

    sendMessage( std::move( response ) );
}

} // namespace stun
} // namespace nx
