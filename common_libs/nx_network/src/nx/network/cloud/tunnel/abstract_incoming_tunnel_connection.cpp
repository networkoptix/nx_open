#include "abstract_incoming_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {

AbstractIncomingTunnelConnection::AbstractIncomingTunnelConnection(
    String connectionId)
:
    m_connectionId(std::move(connectionId))
{
}

} // namespace cloud
} // namespace network
} // namespace nx
