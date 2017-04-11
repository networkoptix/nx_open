#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "time_protocol_connection.h"

namespace nx {
namespace time_server {

class TimeProtocolServer:
    public StreamSocketServer<TimeProtocolServer, TimeProtocolConnection>
{
    typedef StreamSocketServer<TimeProtocolServer, TimeProtocolConnection> base_type;

public:
    TimeProtocolServer(
        bool sslRequired,
        nx::network::NatTraversalSupport natTraversalRequired)
    :
        base_type(sslRequired, natTraversalRequired)
    {
    }

protected:
    virtual std::shared_ptr<ConnectionType> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        return std::make_shared<TimeProtocolConnection>(
            this,
            std::move(_socket));
    }
};

} // namespace time_server
} // namespace nx
