#include "time_protocol_server.h"

namespace nx {
namespace network {

TimeProtocolServer::TimeProtocolServer(bool sslRequired):
    base_type(sslRequired, nx::network::NatTraversalSupport::disabled)
{
}

std::shared_ptr<TimeProtocolServer::ConnectionType> TimeProtocolServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    return std::make_shared<TimeProtocolConnection>(
        this,
        std::move(connection));
}

} // namespace network
} // namespace nx
