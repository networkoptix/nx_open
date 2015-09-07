/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_STREAM_SOCKET_SERVER_H
#define STUN_STREAM_SOCKET_SERVER_H

#include <utils/network/connection_server/stream_socket_server.h>
#include <utils/network/connection_server/stream_socket_server.h>

#include "server_connection.h"

namespace nx {
namespace stun {

class SocketServer
:
    public StreamSocketServer<SocketServer, ServerConnection>
{
    typedef StreamSocketServer<SocketServer, ServerConnection> base_type;

public:
    SocketServer( bool sslRequired,
                  SocketFactory::NatTraversalType natTraversalRequired )
    :
        base_type( sslRequired, natTraversalRequired )
    {
    }

protected:
    virtual std::shared_ptr<ServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        return std::make_shared<ServerConnection>(
            this,
            std::move(_socket) );
    }
};

} // namespace stun
} // namespace nx

#endif  //STUN_STREAM_SOCKET_SERVER_H
