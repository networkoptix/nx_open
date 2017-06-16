#include "time_protocol_server.h"

namespace nx {
namespace network {

TimeProtocolServer::TimeProtocolServer(
    bool sslRequired,
    nx::network::NatTraversalSupport natTraversalRequired)
    :
    base_type(sslRequired, natTraversalRequired)
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
