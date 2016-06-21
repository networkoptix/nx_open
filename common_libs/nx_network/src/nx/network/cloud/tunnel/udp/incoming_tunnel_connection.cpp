#include "incoming_tunnel_connection.h"

#include <nx/utils/log/log.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {

IncomingTunnelConnection::IncomingTunnelConnection(
    std::unique_ptr<IncomingControlConnection> controlConnection)
:
    m_state(SystemError::noError),
    m_controlConnection(std::move(controlConnection)),
    m_serverSocket(new UdtStreamServerSocket)
{
    m_controlConnection->setErrorHandler(
        [this](SystemError::ErrorCode code)
    {
        m_controlConnection.reset();
        NX_LOGX(lm("Control connection error (%1), closing tunnel...")
            .arg(SystemError::toString(code)), cl_logWARNING);

        m_state = code;
        if (m_serverSocket)
            m_serverSocket->pleaseStopSync(); // we are in IO thread

        if (m_acceptHandler)
        {
            const auto handler = std::move(m_acceptHandler);
            m_acceptHandler = nullptr;
            return handler(code, nullptr);
        }
    });

    m_serverSocket->bindToAioThread(m_controlConnection->socket()->getAioThread());
    if (!m_serverSocket->setNonBlockingMode(true) ||
        !m_serverSocket->bind(m_controlConnection->socket()->getLocalAddress()) ||
        !m_serverSocket->listen())
    {
        NX_LOGX(lm("Can not listen on server socket: ")
            .arg(SystemError::getLastOSErrorText()), cl_logWARNING);

        m_state = SystemError::getLastOSErrorCode();
    }
    else
    {
        NX_LOGX(lm("Listening for new connections on %1")
            .arg(m_serverSocket->getLocalAddress().toString()), cl_logDEBUG1);
    }
}

void IncomingTunnelConnection::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractStreamSocket>)> handler)
{
    NX_ASSERT(!m_acceptHandler, Q_FUNC_INFO, "Concurrent accept");
    m_serverSocket->dispatch(
        [this, handler = std::move(handler)]()
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

                const auto handler = std::move(m_acceptHandler);
                m_acceptHandler = nullptr;
                handler(
                    SystemError::noError,
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
