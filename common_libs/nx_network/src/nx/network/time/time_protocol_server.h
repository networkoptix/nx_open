#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "time_protocol_connection.h"

namespace nx {
namespace network {

class NX_NETWORK_API TimeProtocolServer:
    public server::StreamSocketServer<TimeProtocolServer, TimeProtocolConnection>
{
    using base_type =
        server::StreamSocketServer<TimeProtocolServer, TimeProtocolConnection>;

public:
    TimeProtocolServer(bool sslRequired);

protected:
    virtual std::shared_ptr<ConnectionType> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;
};

} // namespace network
} // namespace nx
