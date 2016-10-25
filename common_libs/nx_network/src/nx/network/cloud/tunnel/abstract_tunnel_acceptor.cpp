#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

AbstractTunnelAcceptor:: AbstractTunnelAcceptor():
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

} // namespace cloud
} // namespace network
} // namespace nx
