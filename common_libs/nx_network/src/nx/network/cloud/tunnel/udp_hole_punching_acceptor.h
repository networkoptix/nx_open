#pragma once

#include "abstract_tunnel_acceptor.h"

#include <nx/network/cloud/mediator_connections.h>
#include <nx/network/udt/udt_socket.h>

namespace nx {
namespace network {
namespace cloud {

// TODO #mux comment
class NX_NETWORK_API UdpHolePunchingTunnelAcceptor
:
    public AbstractTunnelAcceptor
{
public:
    UdpHolePunchingTunnelAcceptor(
        std::list<SocketAddress> addresses);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    void initiateConnection();
    bool checkPleaseStop();
    void executeAcceptHandler(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection> connection = nullptr);

    const std::list<SocketAddress> m_addresses;

    QnMutex m_mutex;
    std::unique_ptr<hpm::api::MediatorServerUdpConnection> m_udpMediatorConnection;
    std::unique_ptr<UdtStreamSocket> m_udtConnectionSocket;

    nx::utils::MoveOnlyFunc<void()> m_stopHandler;
    std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> m_acceptHandler;
};

} // namespace cloud
} // namespace network
} // namespace nx
