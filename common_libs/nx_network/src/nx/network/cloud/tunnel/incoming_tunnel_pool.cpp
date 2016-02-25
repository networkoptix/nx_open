#include "incoming_tunnel_pool.h"

#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

IncomingTunnelPool::IncomingTunnelPool(
    aio::AbstractAioThread* ioThread, size_t acceptLimit)
:
    m_acceptLimit(acceptLimit),
    m_terminated(false),
    m_ioThreadSocket(new TCPSocket)
{
    m_ioThreadSocket->bindToAioThread(ioThread);
}

void IncomingTunnelPool::addNewTunnel(
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    std::shared_ptr<AbstractIncomingTunnelConnection> sharedConnection(
        connection.release());

    {
        QnMutexLocker lk(&m_mutex);
        m_pool.emplace(sharedConnection);
    }

    acceptTunnel(std::move(sharedConnection));
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
    std::function<void(std::unique_ptr<AbstractStreamSocket>)> handler,
    boost::optional<unsigned int> timeout)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_acceptHandler, Q_FUNC_INFO, "Multiple accepts are not supported");

    if (!m_acceptedSockets.empty())
    {
        auto socket = std::move(m_acceptedSockets.front());
        m_acceptedSockets.pop_front();

        lock.unlock();
        return handler(std::move(socket));
    }

    m_acceptHandler = std::move(handler);
    if (timeout && *timeout != 0)
        m_ioThreadSocket->registerTimer(
            *timeout, [this](){ callAcceptHandler(true); });
}

void IncomingTunnelPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    BarrierHandler barrier(
        [this, handler = std::move(handler)]() mutable
        {
            m_ioThreadSocket->pleaseStop(std::move(handler));
        });

    for (const auto& tunnel : m_pool)
        tunnel->pleaseStop(barrier.fork());
}

void IncomingTunnelPool::acceptTunnel(
    std::shared_ptr<AbstractIncomingTunnelConnection> connection)
{
    NX_LOGX(lm("accept tunnel %1").arg(connection), cl_logDEBUG1);
    connection->accept(
        [this, connection](
            SystemError::ErrorCode code,
            std::unique_ptr<AbstractStreamSocket> socket)
        {
            {
                QnMutexLocker lock(&m_mutex);
                if (m_terminated)
                    return;

                if (code != SystemError::noError)
                {
                    NX_LOGX(lm("tunnel %1 is brocken: %2")
                        .arg(connection).arg(SystemError::toString(code)),
                        cl_logDEBUG1);

                    m_pool.erase(connection);
                    return;
                }

                if (m_acceptedSockets.size() >= m_acceptLimit)
                {
                    // TODO: #mux Stop accepting tunnel?
                    //  wouldn't really help while we acceptiong other tunnels
                    //  better to implement epoll like system with tunnels and
                    //  their connections
                    NX_LOGX(lm("sockets queue overflow: %1").arg(m_acceptLimit),
                        cl_logWARNING);
                }
                else
                {
                    m_acceptedSockets.push_back(std::move(socket));
                    if (m_acceptHandler)
                        m_ioThreadSocket->post(
                            [this](){ callAcceptHandler(false); });
                }
            }

            acceptTunnel(std::move(connection));
        });
}

void IncomingTunnelPool::callAcceptHandler(bool timeout)
{
    // Cancel all possible posts (we are in corresponding IO thread)
    m_ioThreadSocket->cancelIOSync(aio::etTimedOut);

    QnMutexLocker lock(&m_mutex);
    if (!m_acceptHandler)
        return;

    const auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;

    if (timeout)
    {
        lock.unlock();
        return handler(nullptr);
    }

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop_front();

    lock.unlock();
    return handler(std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
