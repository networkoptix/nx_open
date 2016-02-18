#include "abstract_incoming_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {

AbstractIncomingTunnelConnection::AbstractIncomingTunnelConnection(
    String remotePeerId)
:
    m_remotePeerId(std::move(remotePeerId))
{
}

const String& AbstractIncomingTunnelConnection::getRemotePeerId() const
{
    return m_remotePeerId;
}

} // namespace cloud
} // namespace network
} // namespace nx
