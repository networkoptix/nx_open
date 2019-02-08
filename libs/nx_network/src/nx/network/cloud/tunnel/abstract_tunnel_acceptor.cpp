#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

AbstractTunnelAcceptor::AbstractTunnelAcceptor():
    m_mediatorConnection(nullptr)
{
}

void AbstractTunnelAcceptor::setConnectionInfo(
    String connectionId, String remotePeerId)
{
    m_connectionId = std::move(connectionId);
    m_remotePeerId = std::move(remotePeerId);
}

void AbstractTunnelAcceptor::setMediatorConnection(
    hpm::api::MediatorServerTcpConnection*  connection)
{
    m_mediatorConnection = connection;
}

String AbstractTunnelAcceptor::connectionId() const
{
    return m_connectionId;
}

String AbstractTunnelAcceptor::remotePeerId() const
{
    return m_remotePeerId;
}

} // namespace cloud
} // namespace network
} // namespace nx
