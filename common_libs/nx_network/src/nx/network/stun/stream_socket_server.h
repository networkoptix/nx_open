#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "server_connection.h"

namespace nx {
namespace stun {

class MessageDispatcher;

class SocketServer:
    public nx::network::server::StreamSocketServer<SocketServer, ServerConnection>
{
    using base_type =
        nx::network::server::StreamSocketServer<SocketServer, ServerConnection>;

public:
    SocketServer(
        const MessageDispatcher* dispatcher,
        bool sslRequired,
        network::NatTraversalSupport natTraversalSupport
            = network::NatTraversalSupport::enabled)
        :
        base_type(sslRequired, natTraversalSupport),
        m_dispatcher(dispatcher)
    {
    }

    virtual ~SocketServer() override
    {
        pleaseStopSync();
    }

protected:
    virtual std::shared_ptr<ServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        return std::make_shared<ServerConnection>(
            this,
            std::move(_socket),
            *m_dispatcher);
    }

private:
    const MessageDispatcher* m_dispatcher;
};

} // namespace stun
} // namespace nx
