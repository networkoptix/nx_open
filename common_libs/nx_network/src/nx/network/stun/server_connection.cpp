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
    m_peerAddress( BaseType::getForeignAddress() )
{
}

ServerConnection::~ServerConnection()
{
    // notify, that connection is being destruct
    // NOTE: this is needed only by weak_ptr holders to know, that this
    //       weak_ptr is not valid any more
    if( m_destructHandler )
        m_destructHandler();
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

void ServerConnection::setDestructHandler( std::function< void() > handler )
{
    QnMutexLocker lk( &m_mutex );
    Q_ASSERT_X( !(handler && m_destructHandler), Q_FUNC_INFO,
                "Can not set new hadler while previous is not removed" );

    m_destructHandler = std::move( handler );
}

void ServerConnection::processBindingRequest( Message message )
{
    Message response( stun::Header(
        MessageClass::successResponse,
        bindingMethod, std::move( message.header.transactionId ) ) );

    response.newAttribute< stun::attrs::XorMappedAddress >(
                m_peerAddress.port, m_peerAddress.address.ipv4() );

    sendMessage( std::move( response ) );
}

void ServerConnection::processCustomRequest( Message message )
{
    if( auto disp = MessageDispatcher::instance() )
        if( disp->dispatchRequest( shared_from_this(), std::move(message) ) )
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
