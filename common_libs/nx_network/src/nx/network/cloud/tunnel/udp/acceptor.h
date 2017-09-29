#pragma once

#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/cloud/mediator_server_connections.h>
#include <nx/network/cloud/tunnel/abstract_tunnel_acceptor.h>
#include <nx/network/cloud/tunnel/udp/incoming_control_connection.h>
#include <nx/network/udt/udt_socket.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

/**
 * Sends connectionAck to mediator over UDT, then creates IncomingControlConnection to return it
 * wrapped into IncomingTunnelConnection. In case if control connection can not be estabilished
 * the error is returned.
 */
class NX_NETWORK_API TunnelAcceptor:
    public AbstractTunnelAcceptor
{
public:
    TunnelAcceptor(
        const SocketAddress& mediatorUdpEndpoint,
        std::list<SocketAddress> peerAddresses,
        nx::hpm::api::ConnectionParameters connectionParametes);

    void setUdpRetransmissionTimeout(std::chrono::milliseconds timeout);
    void setUdpMaxRetransmissions(int count);
    void setHolePunchingEnabled(bool value);

    virtual void accept(AcceptHandler handler) override;
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual std::string toString() const override;

private:
    void connectionAckResult(nx::hpm::api::ResultCode code);

    void startUdtConnection(
        std::list<std::unique_ptr<UdtStreamSocket>>::iterator socketIt,
        const SocketAddress& target);

    void executeAcceptHandler(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection> connection = nullptr);

    const std::list<SocketAddress> m_peerAddresses;
    const nx::hpm::api::ConnectionParameters m_connectionParameters;
    SocketAddress m_mediatorUdpEndpoint;
    std::chrono::milliseconds m_udpRetransmissionTimeout;
    int m_udpMaxRetransmissions;
    bool m_holePunchingEnabled;

    std::unique_ptr<hpm::api::MediatorServerUdpConnection> m_udpMediatorConnection;
    std::list<std::unique_ptr<UdtStreamSocket>> m_sockets;
    std::list<std::unique_ptr<IncomingControlConnection>> m_connections;

    AcceptHandler m_acceptHandler;
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
