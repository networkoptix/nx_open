#include "incoming_tunnel_pool.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

IncomingTunnelPool::IncomingTunnelPool(
    aio::AbstractAioThread* aioThread, size_t acceptLimit)
    :
    m_acceptLimit(acceptLimit)
{
    bindToAioThread(aioThread);
}

void IncomingTunnelPool::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
    for (auto& tunnel: m_pool)
        tunnel->bindToAioThread(aioThread);
}

void IncomingTunnelPool::acceptAsync(AcceptCompletionHandler handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        NX_ASSERT(!m_acceptHandler, "Multiple accepts are not supported");
        m_acceptHandler = std::move(handler);
        if (!m_acceptedSockets.empty())
        {
            lock.unlock();
            return post([this]() { callAcceptHandler(SystemError::noError); });
        }
    }

    if (m_acceptTimeout && (*m_acceptTimeout > std::chrono::milliseconds::zero()))
    {
        m_timer.start(
            *m_acceptTimeout,
            [this]() { callAcceptHandler(SystemError::timedOut); });
    }
}

void IncomingTunnelPool::cancelIOSync()
{
    QnMutexLocker lock(&m_mutex);
    m_acceptHandler = nullptr;
}

std::unique_ptr<AbstractStreamSocket> IncomingTunnelPool::getNextSocketIfAny()
{
    QnMutexLocker lock(&m_mutex);
    if (m_acceptedSockets.empty())
        return nullptr;

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop_front();
    return std::move(socket);
}

void IncomingTunnelPool::setAcceptTimeout(
    boost::optional<std::chrono::milliseconds> acceptTimeout)
{
    m_acceptTimeout = acceptTimeout;
}

void IncomingTunnelPool::addNewTunnel(
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    NX_CRITICAL(m_timer.isInSelfAioThread());
    connection->bindToAioThread(getAioThread());
    auto insert = m_pool.emplace(std::move(connection));
    NX_ASSERT(insert.second);
    acceptTunnel(insert.first);
}

void IncomingTunnelPool::stopWhileInAioThread()
{
    m_pool.clear();
    m_timer.pleaseStopSync();
}

void IncomingTunnelPool::acceptTunnel(TunnelIterator connection)
{
    NX_LOGX(lm("accept tunnel %1").arg(connection->get()), cl_logDEBUG2);
    (*connection)->accept(
        [this, connection](
            SystemError::ErrorCode code,
            std::unique_ptr<AbstractStreamSocket> socket)
        {
            if (code != SystemError::noError)
            {
                NX_LOGX(lm("tunnel %1 is broken: %2")
                    .arg(connection->get()).arg(SystemError::toString(code)),
                    cl_logDEBUG1);

                m_pool.erase(connection);
                return;
            }

            NX_ASSERT(socket);

            acceptTunnel(connection);

            {
                QnMutexLocker lock(&m_mutex);
                if (m_acceptedSockets.size() >= m_acceptLimit)
                {
                    // TODO: #mux Stop accepting tunnel?
                    //  wouldn't really help while we accepting other tunnels
                    //  better to implement epoll like system with tunnels and
                    //  their connections
                    NX_LOGX(lm("sockets queue overflow: %1").arg(m_acceptLimit),
                        cl_logWARNING);
                }
                else
                {
                    m_acceptedSockets.push_back(std::move(socket));
                    lock.unlock();
                    callAcceptHandler(SystemError::noError);
                }
            }
        });
}

void IncomingTunnelPool::callAcceptHandler(SystemError::ErrorCode resultCode)
{
    // Cancel all possible posts (we are in corresponding IO thread).
    m_timer.cancelSync();

    QnMutexLocker lock(&m_mutex);
    if (!m_acceptHandler)
        return;

    if (resultCode != SystemError::noError)
    {
        decltype(m_acceptHandler) handler;
        handler.swap(m_acceptHandler);
        lock.unlock();
        return handler(resultCode, nullptr);
    }

    if (m_acceptedSockets.empty())
        return;

    decltype(m_acceptHandler) handler;
    handler.swap(m_acceptHandler);

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop_front();

    lock.unlock();
    socket->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());
    return handler(SystemError::noError, std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
