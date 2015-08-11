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
            switch( message.header.method )
            {
                case stun::BINDING:
                    {
                        stun::Message response( stun::Header(
                            stun::MessageClass::successResponse,
                            stun::BINDING, std::move( message.header.transactionId ) ) );

                        SocketAddress peer( peer_address_ );
                        response.newAttribute< stun::attrs::XorMappedAddress >(
                                    peer.port, peer.address.ipv4() );

                        Q_ASSERT( sendMessage( std::move( response ) ) );
                    }
                    break;

                default:
                    {
                        if( auto disp = STUNMessageDispatcher::instance() )
                            if( disp->dispatchRequest( this, std::move(message) ) )
                                return;

                        stun::Message response( stun::Header(
                            stun::MessageClass::errorResponse,
                            message.header.method,
                            std::move( message.header.transactionId ) ) );

                        // TODO: verify with RFC
                        response.newAttribute< stun::attrs::ErrorDescription >(
                            error::NOT_FOUND, "Method is not supported" );

                        Q_ASSERT( sendMessage( std::move( response ) ) );

                    }
                    break;
            }
            break;

        default:
            Q_ASSERT( false );  //not supported yet
    }
}

} // namespace hpm
} // namespace nx
