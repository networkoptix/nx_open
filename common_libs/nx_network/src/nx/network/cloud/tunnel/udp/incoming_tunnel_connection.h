#pragma once

#include <nx/network/cloud/tunnel/abstract_incoming_tunnel_connection.h>
#include <nx/network/cloud/tunnel/udp/incoming_control_connection.h>
#include <nx/network/udt/udt_socket.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {

class NX_NETWORK_API IncomingTunnelConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    IncomingTunnelConnection(
        std::unique_ptr<IncomingControlConnection> controlConnection);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    SystemError::ErrorCode m_state;
    std::unique_ptr<IncomingControlConnection> m_controlConnection;
    std::unique_ptr<UdtStreamServerSocket> m_serverSocket;
    std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
