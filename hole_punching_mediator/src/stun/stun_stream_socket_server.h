/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_STREAM_SOCKET_SERVER_H
#define STUN_STREAM_SOCKET_SERVER_H

#include <utils/network/connection_server/stream_socket_server.h>

#include "stun_server_connection.h"


class StunStreamSocketServer
:
    public StreamSocketServer<StunStreamSocketServer, StunServerConnection>
{
    typedef StreamSocketServer<StunStreamSocketServer, StunServerConnection> base_type;

public:
    typedef StunServerConnection ConnectionType;

    StunStreamSocketServer( bool sslRequired, SocketFactory::NatTraversalType natTraversalRequired )
    :
        base_type( sslRequired, natTraversalRequired )
    {
    }
};

#endif  //STUN_STREAM_SOCKET_SERVER_H
