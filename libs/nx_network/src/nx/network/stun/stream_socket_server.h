// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "server_connection.h"

namespace nx {
namespace network {
namespace stun {

class AbstractMessageHandler;

class SocketServer:
    public nx::network::server::StreamSocketServer<SocketServer, ServerConnection>
{
    using base_type =
        nx::network::server::StreamSocketServer<SocketServer, ServerConnection>;

public:
    template<typename... Args>
    SocketServer(
        AbstractMessageHandler* messageHandler,
        Args&&... args)
        :
        base_type(std::forward<Args>(args)...),
        m_messageHandler(messageHandler)
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
            std::move(socket), m_messageHandler);
        return connection;
    }

private:
    AbstractMessageHandler* m_messageHandler = nullptr;
};

} // namespace stun
} // namespace network
} // namespace nx
