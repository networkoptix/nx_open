/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_STREAM_SOCKET_SERVER_H
#define STUN_STREAM_SOCKET_SERVER_H

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/connection_server/stream_socket_server.h>

#include "server_connection.h"

namespace nx {
namespace stun {

class NX_NETWORK_API SocketServer
:
    public StreamSocketServer<SocketServer, ServerConnection>
{
    typedef StreamSocketServer<SocketServer, ServerConnection> base_type;

public:
    SocketServer( bool sslRequired,
                  SocketFactory::NatTraversalType natTraversalRequired
                    = SocketFactory::NatTraversalType::nttAuto )
    :
        base_type( sslRequired, natTraversalRequired )
    {
    }

    ~SocketServer()
    {
        pleaseStop();
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
