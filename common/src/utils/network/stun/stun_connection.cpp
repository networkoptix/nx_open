/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_connection.h"


namespace nx_stun
{
    StunClientConnection::StunClientConnection(
        const SocketAddress& stunServerEndpoint,
        bool useSsl,
        SocketFactory::NatTraversalType natTraversalType )
    :
    //    BaseConnectionType( this, nullptr )
        m_stunServerEndpoint( stunServerEndpoint )
    {
        //TODO creating connection

        m_socket.reset( SocketFactory::createStreamSocket( useSsl, natTraversalType ) );
        m_baseConnection.reset( new BaseConnectionType( this, m_socket.get() ) );
        using namespace std::placeholders;
        m_baseConnection->setMessageHandler( [this]( nx_stun::Message&& msg ) { processMessage( std::move(msg) ); } );
    }

    StunClientConnection::~StunClientConnection()
    {
        //TODO
    }

    void StunClientConnection::pleaseStop( std::function<void()>&& completionHandler )
    {
        //TODO
    }

    bool StunClientConnection::openConnection( std::function<void(SystemError::ErrorCode)>&& completionHandler )
    {
        //m_socket->connectAsync( m_stunServerEndpoint, std::bind(&StunClientConnection::onConnectionDone) );

        //TODO
        return false;
    }

    void StunClientConnection::registerIndicationHandler( std::function<void(nx_stun::Message&&)>&& indicationHandler )
    {
        //TODO
    }

    bool StunClientConnection::sendRequest(
        nx_stun::Message&& request,
        std::function<void(SystemError::ErrorCode, nx_stun::Message&&)>&& completionHandler )
    {
        //if( !m_connected )
        //    return m_socket->connectAsync( m_stunServerEndpoint, std::bind(&StunClientConnection::onConnectionDone) );

        //TODO

        return false;
    }

    void StunClientConnection::closeConnection( StunClientConnection* connection )
    {
        //TODO
    }

    void StunClientConnection::processMessage( nx_stun::Message&& msg )
    {
        //TODO
    }
}
