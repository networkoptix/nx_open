#pragma once

#include "../abstract_tunnel_acceptor.h"

#include <nx/network/cloud/mediator_connections.h>
#include <nx/network/udt/udt_socket.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {

// TODO #mux comment
class NX_NETWORK_API TunnelAcceptor
:
    public AbstractTunnelAcceptor
{
public:
    explicit TunnelAcceptor(SocketAddress peerAddress);

    void setUdtConnectTimeout(std::chrono::milliseconds timeout);
    void setUdpRetransmissionTimeout(std::chrono::milliseconds timeout);
    void setUdpMaxRetransmissions(int count);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    void connectionAckResult(nx::hpm::api::ResultCode code);
    void initiateUdtConnection();
    void executeAcceptHandler(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection> connection = nullptr);

    const SocketAddress m_peerAddress;
    std::chrono::milliseconds m_udtConnectTimeout;
    std::chrono::milliseconds m_udpRetransmissionTimeout;
    int m_udpMaxRetransmissions;

    QnMutex m_mutex;
    std::unique_ptr<hpm::api::MediatorServerUdpConnection> m_udpMediatorConnection;
    std::unique_ptr<UdtStreamSocket> m_udtConnectionSocket;

    nx::utils::MoveOnlyFunc<void()> m_stopHandler;
    std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> m_acceptHandler;
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
