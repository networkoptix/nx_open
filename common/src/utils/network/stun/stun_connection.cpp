/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_connection.h"


namespace nx_stun
{
    StunClientConnection::StunClientConnection(
        const SocketAddress& stunServerEndpoint,
        SocketFactory::NatTraversalType natTraversalType )
    :
        BaseConnectionType( this, nullptr )
    {
    }

    StunClientConnection::~StunClientConnection()
    {
        //TODO #ak
    }

    void StunClientConnection::pleaseStop( std::function<void()>&& completionHandler )
    {
        //TODO #ak
    }

    bool StunClientConnection::openConnection( std::function<void(SystemError::ErrorCode)>&& completionHandler )
    {
        //TODO #ak
        return false;
    }

    void StunClientConnection::registerIndicationHandler( std::function<void(nx_stun::Message&&)>&& indicationHandler )
    {
        //TODO #ak
    }

    bool StunClientConnection::sendRequest(
        nx_stun::Message&& request,
        std::function<void(SystemError::ErrorCode, nx_stun::Message&&)>&& completionHandler )
    {
        //TODO #ak
        return false;
    }

    void StunClientConnection::processMessage( nx_stun::Message&& msg )
    {
        //TODO #ak
    }

    void StunClientConnection::closeConnection( StunClientConnection* connection )
    {
        //TODO #ak
    }
}
