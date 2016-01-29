
#include "outgoing_tunnel.h"


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnel::OutgoingTunnel(String peerId)
:
    Tunnel(std::move(peerId))
{
}

void OutgoingTunnel::connect(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    SocketHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    if (m_state == State::kConnected)
    {
        lk.unlock();
        return m_connection->connect(
            timeout,
            std::move(socketAttributes),
            [this, handler](
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




void OutgoingTunnelPool::pleaseStop(std::function<void()> handler)
{
    //TODO #ak
}

std::shared_ptr<OutgoingTunnel> OutgoingTunnelPool::getTunnel(const String& hostName)
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_pool.find(hostName);
    if (it != m_pool.end())
        return it->second;

    const auto tunnel = std::make_shared<OutgoingTunnel>(hostName);
    tunnel->setStateHandler([this, hostName](Tunnel::State state)
    {
        switch (state)
        {
            case Tunnel::State::kClosed:
                removeTunnel(hostName);
                return;
            default:
                return; // nothing to be done
        };
    });

    if (!m_pool.insert(std::make_pair(hostName, tunnel)).second)
    {
        Q_ASSERT(false);
    }
    return tunnel;
}

void OutgoingTunnelPool::removeTunnel(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(m_pool.erase(peerId), Q_FUNC_INFO, "Too late signal delivery??");
}

} // namespace cloud
} // namespace network
} // namespace nx
