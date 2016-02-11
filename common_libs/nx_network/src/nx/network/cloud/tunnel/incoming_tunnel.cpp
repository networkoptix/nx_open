
#include "incoming_tunnel.h"

#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_tunnel_udt.h>


namespace nx {
namespace network {
namespace cloud {

IncomingTunnel::IncomingTunnel(std::unique_ptr<AbstractTunnelConnection> connection)
:
    Tunnel(std::move(connection))
{
}

void IncomingTunnel::accept(SocketHandler handler)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_state != State::kConnected)
        {
            lk.unlock();
            handler(SystemError::notConnected, nullptr);
        }
    }

    m_connection->accept([this, handler](
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractStreamSocket> socket,
        bool stillValid)
    {
        handler(code, std::move(socket));
        if (stillValid)
            return;

        QnMutexLocker lk(&m_mutex);
        changeState(State::kClosed, &lk);
    });
}


IncomingTunnelPool::IncomingTunnelPool()
    : m_acceptLimit(0)
{
}

void IncomingTunnelPool::addNewTunnel(
    std::unique_ptr<AbstractTunnelConnection> connection)
{
    auto tunnel = std::make_shared<IncomingTunnel>(std::move(connection));
    {
        QnMutexLocker lk(&m_mutex);
        m_pool.emplace(tunnel);
    }
    acceptTunnel(std::move(tunnel));
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
    std::function<void(std::unique_ptr<AbstractStreamSocket>)> handler)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_acceptHandler, Q_FUNC_INFO, "Multiple accepts are not supported");

    m_acceptHandler = std::move(handler);
    returnSocketIfAny(&lock);
}

void IncomingTunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier([this, handler]()
    {
        BarrierHandler barrier(std::move(handler));

        QnMutexLocker lock(&m_mutex);
        for (const auto& socket : m_acceptedSockets)
            socket->pleaseStop(barrier.fork());
    });

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel->pleaseStop(barrier.fork());
}

void IncomingTunnelPool::acceptTunnel(std::shared_ptr<IncomingTunnel> tunnel)
{
    NX_LOGX(lm("accept tunnel %1").arg(tunnel), cl_logDEBUG1);
    tunnel->accept([this, tunnel]
        (SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (code != SystemError::noError)
        {
            QnMutexLocker lock(&m_mutex);
            NX_LOGX(lm("tunnel %1 is brocken: %2")
                    .arg(tunnel).arg(SystemError::toString(code)), cl_logDEBUG1);

            m_pool.erase(tunnel);
            return;
        }

        acceptTunnel(std::move(tunnel));

        QnMutexLocker lock(&m_mutex);
        if (m_acceptedSockets.size() >= m_acceptLimit)
        {
            // TODO: #mux Stop accepting tunnel?
            //  wouldn't really help while we acceptiong other tunnels
            //  better to implement epoll like system with tunnels and their connections
            NX_LOGX(lm("sockets queue overflow: %1").arg(m_acceptLimit), cl_logWARNING);
            return;
        }

        m_acceptedSockets.push_back(std::move(socket));
        returnSocketIfAny(&lock);
    });
}

void IncomingTunnelPool::returnSocketIfAny(QnMutexLockerBase* lock)
{
    if (!m_acceptHandler || m_acceptedSockets.empty())
        return; // nothing to indicate

    auto request = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop_front();

    lock->unlock();
    request(std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
