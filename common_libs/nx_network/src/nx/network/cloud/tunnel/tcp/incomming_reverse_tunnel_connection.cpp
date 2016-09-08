#include "incomming_reverse_tunnel_connection.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

IncomingReverseTunnelConnection::IncomingReverseTunnelConnection(
    String selfHostName, String targetHostName, SocketAddress targetEndpoint)
:
    m_selfHostName(std::move(selfHostName)),
    m_targetHostName(std::move(targetHostName)),
    m_targetEndpoint(std::move(targetEndpoint))
{
}

void IncomingReverseTunnelConnection::start(
    aio::AbstractAioThread* aioThread, RetryPolicy policy, StartHandler handler)
{
    m_timer.reset(new RetryTimer(policy));
    m_timer->bindToAioThread(aioThread);

    m_startHandler = std::move(handler);
    m_timer->scheduleNextTry([this](){ spawnConnectorIfNeeded(); });
}

void IncomingReverseTunnelConnection::setHttpTimeouts(nx_http::AsyncHttpClient::Timeouts timeouts)
{
    m_httpTimeouts = timeouts;
}

void IncomingReverseTunnelConnection::accept(AcceptHandler handler)
{
    NX_EXPECT(!m_acceptHandler);
    m_timer->dispatch(
        [this, handler = std::move(handler)]()
        {
            m_acceptHandler = std::move(handler);
            NX_LOGX(lm("monitor sockets(%1) on accept").container(m_sockets), cl_logDEBUG1);
            for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
                monitorSocket(it);
        });
}

void IncomingReverseTunnelConnection::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer->pleaseStop(
        [this, handler = std::move(handler)]()
        {
            m_connectors.clear();
            m_sockets.clear();
            handler();
        });
}

void IncomingReverseTunnelConnection::spawnConnectorIfNeeded()
{
    NX_LOGX(lm("There are %1 connector(s) and %2 socket(s) against %3 pool size")
        .strs(m_connectors.size(), m_sockets.size(), m_expectedPoolSize), cl_logDEBUG1);

    if (m_connectors.size() + m_sockets.size() >= m_expectedPoolSize)
        return;

    const auto connectorIt = m_connectors.insert(
        m_connectors.end(),
        std::make_unique<ReverseConnector>(
            m_selfHostName, m_targetHostName, m_timer->getAioThread()));

    NX_LOGX(lm("Start connector(%1)").arg(*connectorIt), cl_logDEBUG2);
    (*connectorIt)->connect(
        m_targetEndpoint,
        [this, connectorIt](SystemError::ErrorCode code)
        {
            auto connector = std::move(*connectorIt);
            m_connectors.erase(connectorIt);
            NX_LOGX(lm("Connector(%1) event: %2").arg(connector)
                .arg(SystemError::toString(code)), cl_logDEBUG2);

            if (code != SystemError::noError
                && m_timer->scheduleNextTry([this](){ spawnConnectorIfNeeded(); }))
            {
                return; //< ignore, better luck next time
            }

            if (m_startHandler)
            {
                const auto handler = std::move(m_startHandler);
                m_startHandler = nullptr;
                handler(code);
            }

            if (code == SystemError::noError)
                saveConnection(std::move(connector));
        });
}

void IncomingReverseTunnelConnection::saveConnection(
    std::unique_ptr<ReverseConnector> connector)
{
    if (const auto value = connector->getPoolSize())
        m_expectedPoolSize = *value;

    if (const auto value = connector->getKeepAliveOptions())
    {
        if (value != m_keepAliveOptions)
        {
            m_keepAliveOptions = value;
            NX_LOGX(lm("New keepAliveOptions=%1").str(m_keepAliveOptions), cl_logDEBUG1);

            for (auto it = m_sockets.begin(); it != m_sockets.end(); )
            {
                if (!(*it)->setKeepAlive(m_keepAliveOptions))
                {
                    NX_LOGX(lm("Could not set keepAliveOptions=%1 to existed socket(%2): %3")
                        .arg(m_keepAliveOptions).arg(*it)
                        .arg(SystemError::getLastOSErrorText()), cl_logDEBUG1);

                    it = m_sockets.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    auto socket = connector->takeSocket();
    if (!socket)
        return spawnConnectorIfNeeded();

    if (m_keepAliveOptions && !socket->setKeepAlive(m_keepAliveOptions))
    {
        NX_LOGX(lm("Could not set keepAliveOptions=%1 to new socket(%2): %3")
            .arg(m_keepAliveOptions).arg(socket)
            .arg(SystemError::getLastOSErrorText()), cl_logDEBUG1);

        return spawnConnectorIfNeeded();
    }

    const auto socketIt = m_sockets.insert(m_sockets.end(), std::move(socket));
    if (m_acceptHandler)
    {
        NX_LOGX(lm("monitor socket(%1) from connector(%2)")
            .args(*socketIt).arg(connector), cl_logDEBUG2);

        monitorSocket(socketIt);
    }
}

void IncomingReverseTunnelConnection::monitorSocket(
    std::list<std::unique_ptr<BufferedStreamSocket>>::iterator socketIt)
{
    (*socketIt)->catchRecvEvent(
        [this, socketIt](SystemError::ErrorCode code)
        {
            NX_CRITICAL(m_acceptHandler);

            auto socket = std::move(*socketIt);
            m_sockets.erase(socketIt);
            NX_LOGX(lm("socket(%1) event: %2").arg(socket)
                .arg(SystemError::toString(code)), cl_logDEBUG1);

            spawnConnectorIfNeeded();
            if (code != SystemError::noError)
                return;

            const auto handler = std::move(m_acceptHandler);
            m_acceptHandler = nullptr;
            if (socket->setNonBlockingMode(false))
                handler(SystemError::noError, std::move(socket));
            else
                handler(SystemError::getLastOSErrorCode(), nullptr);
        });
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
