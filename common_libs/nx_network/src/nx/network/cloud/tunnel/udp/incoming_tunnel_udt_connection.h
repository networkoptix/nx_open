#pragma once

#include "../abstract_incoming_tunnel_connection.h"

#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/udt/udt_socket.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {

class NX_NETWORK_API IncomingTunnelUdtConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    IncomingTunnelUdtConnection(
        String connectionId,
        std::unique_ptr<UdtStreamSocket> connectionSocket,
        const nx::hpm::api::ConnectionParameters& connectionParameters);

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    void monitorKeepAlive();
    void readConnectionRequest();
    void readRequest();
    void writeResponse();
    void connectionSocketError(SystemError::ErrorCode code);

    const std::chrono::milliseconds m_maxKeepAliveInterval;
    std::chrono::steady_clock::time_point m_lastKeepAlive;
    SystemError::ErrorCode m_state;

    Buffer m_connectionBuffer;
    stun::Message m_connectionMessage;
    stun::MessageParser m_connectionParser;
    std::unique_ptr<UdtStreamSocket> m_connectionSocket;

    std::unique_ptr<UdtStreamServerSocket> m_serverSocket;
    std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
