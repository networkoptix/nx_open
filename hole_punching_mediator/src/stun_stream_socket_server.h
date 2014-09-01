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
public:
    typedef StunServerConnection ConnectionType;
};

#endif  //STUN_STREAM_SOCKET_SERVER_H
