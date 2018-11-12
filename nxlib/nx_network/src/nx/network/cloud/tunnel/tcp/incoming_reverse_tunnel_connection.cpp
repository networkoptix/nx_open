#include "incoming_reverse_tunnel_connection.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

IncomingReverseTunnelConnection::IncomingReverseTunnelConnection(
    String selfHostName,
    String targetHostName,
    SocketAddress proxyServiceEndpoint)
:
    m_selfHostName(std::move(selfHostName)),
    m_targetHostName(std::move(targetHostName)),
    m_proxyServiceEndpoint(std::move(proxyServiceEndpoint))
{
}

void IncomingReverseTunnelConnection::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& connector: m_connectors)
        connector->bindToAioThread(aioThread);

    for (auto& socket: m_sockets)
        socket->bindToAioThread(aioThread);

    if (m_timer)
        m_timer->bindToAioThread(aioThread);
}

void IncomingReverseTunnelConnection::start(
    RetryPolicy policy,
    StartHandler handler)
{
    NX_ASSERT(policy.maxRetryCount > 0); //< TODO: #ak Should refactor and remove this assert.
    m_startHandler = std::move(handler);

    m_timer.reset(new RetryTimer(policy));
    m_timer->bindToAioThread(getAioThread());
    m_timer->scheduleNextTry([this](){ spawnConnectorIfNeeded(); });
}

void IncomingReverseTunnelConnection::setHttpTimeouts(
    nx::network::http::AsyncClient::Timeouts timeouts)
{
    m_httpTimeouts = timeouts;
}

void IncomingReverseTunnelConnection::accept(AcceptHandler handler)
{
    NX_ASSERT(!m_acceptHandler);
    dispatch(
        [this, handler = std::move(handler)]()
        {
            if (isExhausted())
                return handler(SystemError::notConnected, nullptr);

            m_acceptHandler = std::move(handler);
            NX_DEBUG(this, lm("Monitor sockets(%1) on accept").container(m_sockets));
            for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
                monitorSocket(it);
        });
}

void IncomingReverseTunnelConnection::stopWhileInAioThread()
{
    m_connectors.clear();
    m_sockets.clear();
    m_timer.reset();
}

void IncomingReverseTunnelConnection::spawnConnectorIfNeeded()
{
    NX_DEBUG(this, lm("There are %1 connector(s) and %2 socket(s) against %3 pool size")
        .args(m_connectors.size(), m_sockets.size(), m_expectedPoolSize));

    if (m_connectors.size() + m_sockets.size() >= m_expectedPoolSize)
        return;

    auto connector = std::make_unique<ReverseConnector>(
        m_selfHostName,
        m_targetHostName);
    connector->bindToAioThread(getAioThread());
    const auto connectorIt = m_connectors.insert(
        m_connectors.end(),
        std::move(connector));

    NX_VERBOSE(this, lm("Start connector(%1)").arg(*connectorIt));
    (*connectorIt)->connect(
        m_proxyServiceEndpoint,
        [this, connectorIt](SystemError::ErrorCode code)
        {
            auto connector = std::move(*connectorIt);
            m_connectors.erase(connectorIt);
            NX_VERBOSE(this, lm("Connector(%1) event: %2").arg(connector)
                .arg(SystemError::toString(code)));

            if (code != SystemError::noError)
            {
                if (m_timer->timeToEvent())
                    return; //< Retry is already scheduled.

                if (m_timer->scheduleNextTry([this](){ spawnConnectorIfNeeded(); }))
                {
                    NX_VERBOSE(this, lm("Sheduled retry in %1").arg(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            *m_timer->timeToEvent())));
                    return;
                }
            }

            nx::utils::ObjectDestructionFlag::Watcher thisDestructionWatcher(&m_destructionFlag);
            utils::moveAndCallOptional(m_startHandler, code);
            if (thisDestructionWatcher.objectDestroyed())
                return;

            if (code == SystemError::noError)
            {
                m_timer->cancelSync();
                m_timer->reset();
                return saveConnection(std::move(connector));
            }

            if (isExhausted())
            {
                NX_DEBUG(this, lm("Exhausted tunnel: %1").arg(SystemError::toString(code)));
                utils::moveAndCallOptional(m_acceptHandler, code, nullptr);
            }
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
            NX_DEBUG(this, lm("New keepAliveOptions=%1").arg(m_keepAliveOptions));

            for (auto it = m_sockets.begin(); it != m_sockets.end(); )
            {
                if (!(*it)->setKeepAlive(m_keepAliveOptions))
                {
                    NX_DEBUG(this, lm("Could not set keepAliveOptions=%1 to existed socket(%2): %3")
                        .arg(m_keepAliveOptions).arg(*it)
                        .arg(SystemError::getLastOSErrorText()));

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
        NX_DEBUG(this, lm("Could not set keepAliveOptions=%1 to new socket(%2): %3")
            .arg(m_keepAliveOptions).arg(socket)
            .arg(SystemError::getLastOSErrorText()));

        return spawnConnectorIfNeeded();
    }

    const auto socketIt = m_sockets.insert(m_sockets.end(), std::move(socket));
    if (m_acceptHandler)
    {
        NX_VERBOSE(this, lm("Monitor socket(%1) from connector(%2)")
            .args(*socketIt).arg(connector));

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
            NX_DEBUG(this, lm("Socket(%1) event: %2").arg(socket)
                .arg(SystemError::toString(code)));

            spawnConnectorIfNeeded();
            if (code != SystemError::noError)
            {
                if (isExhausted())
                    utils::moveAndCall(m_acceptHandler, code, nullptr);

                return;
            }

            if (socket->setNonBlockingMode(false))
                utils::moveAndCall(m_acceptHandler, SystemError::noError, std::move(socket));
            else
                utils::moveAndCall(m_acceptHandler, SystemError::getLastOSErrorCode(), nullptr);
        });
}

bool IncomingReverseTunnelConnection::isExhausted() const
{
    return (m_connectors.size() == 0) && (m_sockets.size() == 0)
        && (m_timer->retriesLeft() == 0) && !m_timer->timeToEvent();
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
