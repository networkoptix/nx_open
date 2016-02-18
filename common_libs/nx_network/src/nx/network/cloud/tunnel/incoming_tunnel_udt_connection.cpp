#include "incoming_tunnel_udt_connection.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

IncomingTunnelUdtConnection::IncomingTunnelUdtConnection(
    String remotePeerId, std::unique_ptr<UdtStreamSocket> connectionSocket)
:
    AbstractIncomingTunnelConnection(std::move(remotePeerId)),
    m_connectionSocket(std::move(connectionSocket)),
    m_serverSocket(new UdtStreamServerSocket)
{
    // TODO: read for UdpHolePunchingSyn on m_serverSocket and send back
    //      UdpHolePunchingSyn

    m_serverSocket->bindToAioThread(m_connectionSocket->getAioThread());
    if (!m_serverSocket->setNonBlockingMode(true) ||
        !m_serverSocket->bind(m_connectionSocket->getLocalAddress()) ||
        !m_serverSocket->listen())
    {
        m_serverSocket.reset();
    }
}

void IncomingTunnelUdtConnection::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractStreamSocket>)> handler)
{
    if (!m_serverSocket)
        handler(SystemError::connectionReset, nullptr);

    m_acceptHandler = std::move(handler);
    m_serverSocket->acceptAsync(
        [this](SystemError::ErrorCode code,
               AbstractStreamSocket* socket)
    {
        NX_LOGX(lm("Accepted %1: %2").arg(socket)
            .arg(SystemError::toString(code)), cl_logDEBUG2);

        const auto handler = std::move(m_acceptHandler);
        m_acceptHandler = nullptr;
        handler(
            SystemError::noError,
            std::unique_ptr<AbstractStreamSocket>(socket));
    });
}

void IncomingTunnelUdtConnection::pleaseStop(
    std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));
    m_connectionSocket->pleaseStop(barrier.fork());
    if (m_serverSocket)
        m_serverSocket->pleaseStop(barrier.fork());
}

} // namespace cloud
} // namespace network
} // namespace nx
