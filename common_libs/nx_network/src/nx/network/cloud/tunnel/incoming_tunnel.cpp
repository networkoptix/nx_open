
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

void IncomingTunnelPool::addNewTunnel(std::unique_ptr<AbstractTunnelConnection> connection)
{
    auto tunnel = std::make_shared<IncomingTunnel>(std::move(connection));
    {
        QnMutexLocker lk(&m_mutex);
        m_pool.emplace(tunnel);
    }
    acceptTunnel(std::move(tunnel));
}

void IncomingTunnelPool::acceptNewSocket(Tunnel::SocketHandler handler)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_acceptRequest, Q_FUNC_INFO, "Multiple accepts are not supported");

    m_acceptRequest = std::move(handler);
    indicateFirstSocket(&lock);
}

void IncomingTunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier([this, handler]()
    {
        QnMutexLocker lock(&m_mutex);
        if (const auto socket = std::move(m_indicatingSocket))
        {
            m_indicatingSocket = nullptr;
            lock.unlock();
            return socket->pleaseStop(std::move(handler));
        }

        lock.unlock();
        handler();
    });

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel->pleaseStop(barrier.fork());
}

void IncomingTunnelPool::acceptTunnel(std::shared_ptr<IncomingTunnel> tunnel)
{
    tunnel->accept([this, tunnel]
        (SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (code != SystemError::noError)
        {
            QnMutexLocker lock(&m_mutex);
            m_pool.erase(tunnel);
            return;
        }

        acceptTunnel(std::move(tunnel));

        QnMutexLocker lock(&m_mutex);
        m_acceptedSockets.push(std::move(socket));
        indicateFirstSocket(&lock);
    });
}

void IncomingTunnelPool::indicateFirstSocket(QnMutexLockerBase* lock)
{
    if (m_indicatingSocket)
        return; // indication is in progress

    if (!m_acceptRequest || m_acceptedSockets.empty())
        return; // nothing to indicate

    m_indicatingSocket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop();
    lock->unlock();

    m_indicatingSocket->sendAsync(
        Tunnel::ACCEPT_INDICATION,
        [&](SystemError::ErrorCode code, size_t size)
        {
            QnMutexLocker lock(&m_mutex);
            if (code != SystemError::noError ||
                size != Tunnel::ACCEPT_INDICATION.size())
            {
                // TODO: #mux NX_LOG

                return indicateFirstSocket(&lock);
            }

            auto request = std::move(*m_acceptRequest);
            m_acceptRequest = nullptr;

            auto socket = std::move(m_indicatingSocket);
            m_indicatingSocket = nullptr;

            lock.unlock();
            request(SystemError::noError, std::move(socket));
        });
}

} // namespace cloud
} // namespace network
} // namespace nx
