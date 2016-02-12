#pragma once

#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

// TODO #mux comment
class UdpHolePunchingTunnelAcceptor
:
    public AbstractTunnelAcceptor
{
public:
    UdpHolePunchingTunnelAcceptor(
        String connectionId, String localPeerId, String remotePeerId,
        std::list<SocketAddress> addresses);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) override;

    void pleaseStop(std::function<void()> handler) override;

private:
    const String m_connectionId;
    const String m_localPeerId;
    const String m_remotePeerId;
    const std::list<SocketAddress> m_addresses;
};

} // namespace cloud
} // namespace network
} // namespace nx
