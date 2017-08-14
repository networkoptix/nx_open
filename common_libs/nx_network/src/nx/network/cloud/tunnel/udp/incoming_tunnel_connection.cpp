#include "incoming_tunnel_connection.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

IncomingTunnelConnection::IncomingTunnelConnection(
    std::unique_ptr<IncomingControlConnection> controlConnection,
    std::unique_ptr<AbstractStreamServerSocket> serverSocket)
:
    m_state(SystemError::noError),
    m_controlConnection(std::move(controlConnection)),
    m_serverSocket(std::move(serverSocket))
{
    if (!m_serverSocket)
        m_serverSocket = std::make_unique<UdtStreamServerSocket>(AF_INET);

    auto controlSocket = m_controlConnection->socket();
    m_serverSocket->bindToAioThread(controlSocket->getAioThread());

    m_controlConnection->setErrorHandler(
        [this](SystemError::ErrorCode code)
        {
            m_controlConnection.reset();
            NX_LOGX(lm("Control connection error (%1), closing tunnel...")
                .arg(SystemError::toString(code)), cl_logDEBUG1);

            m_state = code;
            if (m_serverSocket)
                m_serverSocket->pleaseStopSync(false); //< We are in AIO thread.

            if (m_acceptHandler)
                nx::utils::swapAndCall(m_acceptHandler, code, nullptr);
        });

    const SocketAddress addressToBind(HostAddress::anyHost, controlSocket->getLocalAddress().port);
    if (!m_serverSocket->setNonBlockingMode(true) ||
        !m_serverSocket->bind(addressToBind) ||
        !m_serverSocket->listen())
    {
        NX_LOGX(lm("Can not listen on server socket %1: %2")
            .strs(addressToBind, SystemError::getLastOSErrorText()), cl_logWARNING);

        m_state = SystemError::getLastOSErrorCode();
    }
    else
    {
        NX_LOGX(lm("Listening for new connections on %1")
            .arg(m_serverSocket->getLocalAddress().toString()), cl_logDEBUG1);
    }
}

void IncomingTunnelConnection::accept(AcceptHandler handler)
{
    NX_ASSERT(!m_acceptHandler, Q_FUNC_INFO, "Concurrent accept");
    m_serverSocket->post(
        [this, handler = std::move(handler)]() mutable
        {
            if (m_state != SystemError::noError)
                return handler(m_state, nullptr);

            m_acceptHandler = std::move(handler);
            m_serverSocket->acceptAsync(
                [this](SystemError::ErrorCode code, AbstractStreamSocket* socket)
                {
                    NX_LOGX(lm("Accepted %1 (%2)")
                        .arg(socket).arg(SystemError::toString(code)), cl_logDEBUG2);

                    if (code != SystemError::noError)
                        m_state = code;
                    else
                        m_controlConnection->resetLastKeepAlive();

                    nx::utils::swapAndCall(
                        m_acceptHandler,
                        code,
                        std::unique_ptr<AbstractStreamSocket>(socket));
                });
        });
}

void IncomingTunnelConnection::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_serverSocket->pleaseStop(
        [this, handler = std::move(handler)]()
        {
            m_controlConnection.reset();
            handler();
        });
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
