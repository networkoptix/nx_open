#include "time_protocol_server.h"

namespace nx {
namespace network {

TimeProtocolServer::TimeProtocolServer(bool sslRequired):
    base_type(sslRequired, nx::network::NatTraversalSupport::disabled)
{
}

std::shared_ptr<TimeProtocolServer::ConnectionType> TimeProtocolServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> socket)
{
    return std::make_shared<TimeProtocolConnection>(std::move(socket));
}

} // namespace network
} // namespace nx
