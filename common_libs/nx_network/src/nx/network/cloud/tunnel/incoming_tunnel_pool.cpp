#include "incoming_tunnel_pool.h"

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
    m_aioThread.bindToAioThread(aioThread);
}

void IncomingTunnelPool::addNewTunnel(
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    NX_CRITICAL(m_aioThread.isInSelfAioThread());
    auto insert = m_pool.emplace(std::move(connection));
    NX_ASSERT(insert.second);
    acceptTunnel(insert.first);
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

void IncomingTunnelPool::getNextSocketAsync(
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler,
    boost::optional<unsigned int> timeout)
{
    {
        QnMutexLocker lock(&m_mutex);
        NX_ASSERT(!m_acceptHandler, Q_FUNC_INFO, "Multiple accepts are not supported");
        m_acceptHandler = std::move(handler);
        if (!m_acceptedSockets.empty())
        {
            lock.unlock();
            return m_aioThread.dispatch([this](){ callAcceptHandler(false); });
        }
    }

    if (timeout && *timeout != 0)
    {
        m_aioThread.start(
            std::chrono::milliseconds(*timeout),
            [this](){ callAcceptHandler(true); });
    }
}

void IncomingTunnelPool::cancelAccept()
{
    QnMutexLocker lock(&m_mutex);
    m_acceptHandler = nullptr;
}

void IncomingTunnelPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThread.cancelAsync(
        [this, handler = std::move(handler)]()
        {
            m_pool.clear();
            handler();
        });
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
                    callAcceptHandler(false);
                }
            }
        });
}

void IncomingTunnelPool::callAcceptHandler(bool timeout)
{
    // Cancel all possible posts (we are in corresponding IO thread)
    m_aioThread.cancelSync();

    QnMutexLocker lock(&m_mutex);
    if (!m_acceptHandler)
        return;

    if (timeout)
    {
        const auto handler = std::move(m_acceptHandler);
        m_acceptHandler = nullptr;
        lock.unlock();
        return handler(nullptr);
    }

    if (m_acceptedSockets.empty())
        return;

    const auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop_front();

    lock.unlock();
    return handler(std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
