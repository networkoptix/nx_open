#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "server_connection.h"

namespace nx {
namespace network {
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
        std::unique_ptr<AbstractStreamSocket> socket) override
    {
        auto connection = std::make_shared<ServerConnection>(
            std::move(socket),
            *m_dispatcher);
        return connection;
    }

private:
    const MessageDispatcher* m_dispatcher;
};

} // namespace stun
} // namespace network
} // namespace nx
