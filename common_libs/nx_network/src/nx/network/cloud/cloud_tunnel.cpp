#include "cloud_tunnel.h"

#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_tunnel_udt.h>

namespace nx {
namespace network {
namespace cloud {

Tunnel::Tunnel(String peerId)
    : m_remotePeerId(std::move(peerId))
    , m_state(State::kInit)
{
}

Tunnel::Tunnel(std::unique_ptr<AbstractTunnelConnection> connection)
    : m_remotePeerId(connection->getRemotePeerId())
    , m_state(State::kConnected)
    , m_connection(std::move(connection))
{
}

QString Tunnel::stateToString(State state)
{
    switch (state)
    {
        case State::kInit:          return lm("init");
        case State::kConnecting:    return lm("connecting");
        case State::kConnected:     return lm("connected");
        case State::kClosed:        return lm("closed");
    }

    return lm("unknown(%1)").arg(static_cast<int>(state));
}

void Tunnel::addConnectionTypes(std::vector<CloudConnectType> types)
{
    QnMutexLocker lk(&m_mutex);
    if (m_state == State::kConnected || m_state == State::kClosed)
        return; // it's too late

    for (const auto type : types)
    {
        if (m_connectors.count(type))
            return; // known type

        std::unique_ptr<AbstractTunnelConnector> connector;
        switch(type)
        {
            case CloudConnectType::udtHp:
                connector = std::make_unique<UdtTunnelConnector>(m_remotePeerId);

            default:
                // Probably just log the ERROR
                Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported CloudConnectType value!");
        };

        if (connector)
        {
            // TODO: #mux Take care of type and connector->getPriority()
            m_connectors.emplace(type, std::move(connector));
        }
    }
}

void Tunnel::setStateHandler(std::function<void(State)> handler)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_stateHandler, Q_FUNC_INFO, "State handler is already set");
    m_stateHandler = std::move(handler);
}

void Tunnel::connect(std::chrono::milliseconds timeout,
                     SocketHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    if (m_state == State::kConnected)
    {
        lk.unlock();
        return m_connection->connect(timeout, [this, handler]
            (SystemError::ErrorCode code,
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

    if (m_state == State::kClosed)
    {
        lk.unlock();
        return handler(SystemError::connectionRefused, nullptr);
    }

    const auto timeoutTimePoint = timeout.count()
            ? Clock::now() + timeout : Clock::time_point::max();
    m_connectHandlers.emplace(timeoutTimePoint, std::move(handler));
    // TODO: #mux registerTimer(m_connectHandlers.begin()->first - Clock::now())

    if (m_state == State::kConnecting)
        return; // nothing to do...

    if (m_state == State::kInit && !m_connectors.empty())
    {
        // TODO: #mux call connect on every connector in m_connectors
        return changeState(State::kConnecting, &lk);
    }

    Q_ASSERT_X(false, Q_FUNC_INFO, lm("Unexpected state %1")
        .arg(stateToString(m_state)).toStdString().c_str());
}

void Tunnel::changeState(State state, QnMutexLockerBase* lock)
{
    Q_ASSERT_X(m_state <= state, Q_FUNC_INFO,
               lm("State is not supposed to downgrade from %1 to %2")
               .arg(stateToString(m_state)).arg(stateToString(state))
               .toStdString().c_str());

    if (m_state == state)
        return;

    const auto handler = m_stateHandler;
    lock->unlock();

    if (handler)
        handler(state);
}

void Tunnel::accept(SocketHandler handler)
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

void Tunnel::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& connector : m_connectors)
        connector.second->pleaseStop(barrier.fork());

    if (m_connection)
        m_connection->pleaseStop(barrier.fork());
}

void TunnelPool::connect(const String& peerId,
                         std::vector<CloudConnectType> connectTypes,
                         std::shared_ptr<StreamSocketOptions> options,
                         Tunnel::SocketHandler handler)
{
    std::chrono::milliseconds timeout(0);
    if (const auto to = options->recvTimeout)
        timeout = std::chrono::milliseconds(*to);

    const auto tunnel = getTunnel(peerId);
    tunnel->addConnectionTypes(connectTypes);
    tunnel->connect(timeout, [this, peerId, handler, options]
        (SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (code != SystemError::noError)
            return handler(code, nullptr);

        SocketRequest request = { std::move(options), std::move(handler) };
        waitConnectingSocket(std::move(socket), std::move(request));
    });
}

void TunnelPool::newTunnel(std::unique_ptr<AbstractTunnelConnection> connection)
{
    auto tunnel = std::make_unique<Tunnel>(std::move(connection));
    // TODO: #mux tunnel->onDie([](){ remove from map });

    QnMutexLocker lk(&m_mutex);
    const auto insert = m_pool.emplace(tunnel->getRemotePeerId(), std::move(tunnel));
    Q_ASSERT(insert.second);
}

void TunnelPool::accept(
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
            // TODO: #mux The timeout timer shell be set
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

void TunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel.second->pleaseStop(barrier.fork());
}

std::shared_ptr<Tunnel> TunnelPool::getTunnel(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_pool.find(peerId);
    if (it != m_pool.end())
        return it->second;

    const auto tunnel = std::make_shared<Tunnel>(peerId);
    tunnel->setStateHandler([this, peerId](Tunnel::State state)
    {
        switch (state)
        {
            case Tunnel::State::kConnected: return acceptTunnel(peerId);
            case Tunnel::State::kClosed:    return removeTunnel(peerId);
            default:                        return; // nothing to be done
        };
    });

    Q_ASSERT(m_pool.insert(std::make_pair(peerId, tunnel)).second);
    return tunnel;
}

void TunnelPool::acceptTunnel(const String& peerId)
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

void TunnelPool::removeTunnel(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(m_pool.erase(peerId), Q_FUNC_INFO, "Too late signal delivery??");
}

void TunnelPool::waitConnectingSocket(
        std::unique_ptr<AbstractStreamSocket> socket,
        SocketRequest request)
{
    // TODO: #mux Here socket shell wait for indication and call handler
    //            only after it's recieved (apply result shell also be checked)

    // TODO: #mux Replace assert with error handling
    Q_ASSERT(request.options->apply(socket.get()));
    request.handler(SystemError::noError, std::move(socket));
}

void TunnelPool::indicateAcceptedSocket(
        std::unique_ptr<AbstractStreamSocket> socket,
        SocketRequest request)
{
    // TODO: #mux Here socket shell write indication and call handler
    //            only after it's recieved (apply result shell also be checked)

    // TODO: #mux Replace assert with error handling
    Q_ASSERT(request.options->apply(socket.get()));
    request.handler(SystemError::noError, std::move(socket));
}

} // namespace cloud
} // namespace network
} // namespace nx
