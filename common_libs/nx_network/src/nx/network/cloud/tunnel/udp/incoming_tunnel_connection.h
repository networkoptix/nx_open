#pragma once

#include <nx/network/cloud/tunnel/abstract_incoming_tunnel_connection.h>
#include <nx/network/cloud/tunnel/udp/incoming_control_connection.h>
#include <nx/network/udt/udt_socket.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {

/**
 * Creates UDT server socket on the IncomingControlConnection's port to accept direct connections
 * while control connection is alive. The error is reported by accept handler as soon as control
 * connection dies.
 */
class NX_NETWORK_API IncomingTunnelConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    IncomingTunnelConnection(
        std::unique_ptr<IncomingControlConnection> controlConnection,
        std::unique_ptr<AbstractStreamServerSocket> serverSocket = nullptr);

    void accept(AcceptHandler handler) override;
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    SystemError::ErrorCode m_state;
    std::unique_ptr<IncomingControlConnection> m_controlConnection;
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    AcceptHandler m_acceptHandler;
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
