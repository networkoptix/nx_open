
#include "cloud_tunnel.h"

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
    auto tunnel = std::make_unique<IncomingTunnel>(std::move(connection));
    // TODO: #mux tunnel->onDie([](){ remove from map });

    QnMutexLocker lk(&m_mutex);
    const auto insert = m_pool.emplace(tunnel->getRemotePeerId(), std::move(tunnel));
    Q_ASSERT(insert.second);
}

void IncomingTunnelPool::accept(
        std::shared_ptr<StreamSocketOptions> options,
        Tunnel::SocketHandler handler)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_acceptRequest, Q_FUNC_INFO, "Multiple accepts are not supported");

    SocketRequest request = { std::move(options), std::move(handler) };
    if (m_acceptedSockets.empty())
    {
        if (options->nonBlockingMode && *options->nonBlockingMode)
        {
            lock.unlock();
            return handler(SystemError::wouldBlock, nullptr);
        }

        if (const auto timeout = options->recvTimeout)
        {
            // TODO: #mux The timeout timer shall be set
            static_cast<void>(*timeout);
        }

        m_acceptRequest = std::move(request);
        return;
    }

    auto socket = std::move(m_acceptedSockets.front());
    m_acceptedSockets.pop();

    lock.unlock();
    return indicateAcceptedSocket(std::move(socket), std::move(request));
}

void IncomingTunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel.second->pleaseStop(barrier.fork());
}

void IncomingTunnelPool::acceptTunnel(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_pool.find(peerId);
    if (it == m_pool.end())
        return;

    it->second->accept([this, peerId]
        (SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
    {
        acceptTunnel(peerId);
        if (code != SystemError::noError)
            return;

        QnMutexLocker lock(&m_mutex);
        if (!m_acceptRequest)
            return m_acceptedSockets.push(std::move(socket));

        auto request = std::move(*m_acceptRequest);
        m_acceptRequest = boost::none;

        lock.unlock();
        indicateAcceptedSocket(std::move(socket), std::move(request));
    });
}

void IncomingTunnelPool::removeTunnel(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(m_pool.erase(peerId), Q_FUNC_INFO, "Too late signal delivery??");
}

void IncomingTunnelPool::waitConnectingSocket(
        std::unique_ptr<AbstractStreamSocket> socket,
        SocketRequest request)
{
    // TODO: #mux Here socket shall wait for indication and call handler
    //            only after it's recieved (apply result shall also be checked)

    // TODO: #mux Replace assert with error handling
    Q_ASSERT(request.options->apply(socket.get()));
    request.handler(SystemError::noError, std::move(socket));
}

void IncomingTunnelPool::indicateAcceptedSocket(
        std::unique_ptr<AbstractStreamSocket> socket,
        SocketRequest request)
{
    // TODO: #mux Here socket shall write indication and call handler
    //            only after it's recieved (apply result shall also be checked)

    // TODO: #mux Replace assert with error handling
    Q_ASSERT(request.options->apply(socket.get()));
    request.handler(SystemError::noError, std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
