
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

IncomingTunnelPool::IncomingTunnelPool(
    aio::AbstractAioThread* ioThread, size_t acceptLimit)
:
    m_acceptLimit(acceptLimit),
    m_ioThreadSocket(new TCPSocket)
{
    m_ioThreadSocket->bindToAioThread(ioThread);
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

void IncomingTunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier([this, handler]()
    {
        m_ioThreadSocket->pleaseStop(std::move(handler));
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
        if (m_acceptHandler)
            m_ioThreadSocket->post([this](){ callAcceptHandler(false); });
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
