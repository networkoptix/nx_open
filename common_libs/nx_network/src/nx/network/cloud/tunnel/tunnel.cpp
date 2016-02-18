
#include "tunnel.h"

#include <nx/network/socket_global.h>


namespace nx {
namespace network {
namespace cloud {

Tunnel::Tunnel(String remotePeerId)
:
    m_state(State::kInit),
    m_remotePeerId(std::move(remotePeerId))
{
}

Tunnel::Tunnel(std::unique_ptr<AbstractTunnelConnection> connection)
    : m_state(State::kConnected)
    , m_connection(std::move(connection))
    , m_remotePeerId(m_connection->getRemotePeerId())
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

//void Tunnel::addConnectionTypes(std::vector<CloudConnectType> types)
//{
//    QnMutexLocker lk(&m_mutex);
//    if (m_state == State::kConnected || m_state == State::kClosed)
//        return; // it's too late
//
//    for (const auto type : types)
//    {
//        if (m_connectors.count(type))
//            return; // known type
//
//        std::unique_ptr<AbstractTunnelConnector> connector;
//        switch (type)
//        {
//            case CloudConnectType::udtHp:
//                connector = std::make_unique<UdtTunnelConnector>(m_remotePeerId);
//
//            default:
//                // Probably just log the ERROR
//                Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported CloudConnectType value!");
//        };
//
//        if (connector)
//        {
//            // TODO: #mux Take care of type and connector->getPriority()
//            m_connectors.emplace(type, std::move(connector));
//        }
//    }
//}

void Tunnel::setStateHandler(std::function<void(State)> handler)
{
    QnMutexLocker lock(&m_mutex);
    Q_ASSERT_X(!m_stateHandler, Q_FUNC_INFO, "State handler is already set");
    m_stateHandler = std::move(handler);
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

void Tunnel::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
//    for (const auto& connector : m_connectors)
//        connector.second->pleaseStop(barrier.fork());

    if (m_connection)
        m_connection->pleaseStop(barrier.fork());
}

} // namespace cloud
} // namespace network
} // namespace nx
