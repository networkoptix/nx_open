#include "udp_hole_punching_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

UdpHolePunchingTunnelAcceptor::UdpHolePunchingTunnelAcceptor(
    String connectionId, String localPeerId, String remotePeerId,
    std::list<SocketAddress> addresses)
:
    m_connectionId(std::move(connectionId)),
    m_localPeerId(std::move(localPeerId)),
    m_remotePeerId(std::move(remotePeerId)),
    m_addresses(std::move(addresses))
{
}

void UdpHolePunchingTunnelAcceptor::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractIncomingTunnelConnection>)> handler)
{
    // TODO: listen for mediator indcations and initiate UdtTunnelConnection
    static_cast<void>(handler);
}

void UdpHolePunchingTunnelAcceptor::pleaseStop(
    std::function<void()> handler)
{
    // TODO: cancel all async operations
    static_cast<void>(handler);
}

} // namespace cloud
} // namespace network
} // namespace nx
