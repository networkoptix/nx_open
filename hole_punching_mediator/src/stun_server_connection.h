/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_SERVER_CONNECTION_H
#define STUN_SERVER_CONNECTION_H

#include "stun/base_stun_connection.h"


class StunStreamSocketServer;

class StunServerConnection
:
    public nx_stun::BaseStunServerConnection<StunServerConnection, StunStreamSocketServer>
{
public:
    typedef nx_stun::BaseStunServerConnection<StunServerConnection, StunStreamSocketServer> BaseType;

    StunServerConnection(
        StunStreamSocketServer* socketServer,
        AbstractStreamSocket* sock );

    void processMessage( const nx_stun::Message& request, boost::optional<nx_stun::Message>* const response );
};

#endif  //STUN_SERVER_CONNECTION_H
