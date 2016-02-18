#pragma once

#include "abstract_incoming_tunnel_connection.h"

#include <nx/network/udt/udt_socket.h>

namespace nx {
namespace network {
namespace cloud {

class IncomingTunnelUdtConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    IncomingTunnelUdtConnection(
        String remotePeerId, std::unique_ptr<UdtStreamSocket> connectionSocket);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    std::unique_ptr<UdtStreamSocket> m_connectionSocket;
    std::unique_ptr<UdtStreamServerSocket> m_serverSocket;
    std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
};

} // namespace cloud
} // namespace network
} // namespace nx
