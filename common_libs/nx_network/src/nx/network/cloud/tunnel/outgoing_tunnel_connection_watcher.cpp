#include "outgoing_tunnel_connection_watcher.h"

#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelConnectionWatcher::OutgoingTunnelConnectionWatcher(
    nx::hpm::api::ConnectionParameters connectionParameters,
    std::unique_ptr<AbstractOutgoingTunnelConnection> tunnelConnection)
    :
    m_connectionParameters(std::move(connectionParameters)),
    m_tunnelConnection(std::move(tunnelConnection)),
    m_inactivityTimer(std::make_unique<aio::Timer>())
{
    bindToAioThread(SocketGlobals::aioService().getCurrentAioThread());
}

OutgoingTunnelConnectionWatcher::~OutgoingTunnelConnectionWatcher()
{
    stopWhileInAioThread();
}

void OutgoingTunnelConnectionWatcher::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_tunnelConnection->bindToAioThread(aioThread);
    m_inactivityTimer->bindToAioThread(aioThread);
}

void OutgoingTunnelConnectionWatcher::stopWhileInAioThread()
{
    m_tunnelConnection.reset();
    m_inactivityTimer.reset();
}

void OutgoingTunnelConnectionWatcher::start()
{
    launchInactivityTimer();
    m_tunnelConnection->start();
}

void OutgoingTunnelConnectionWatcher::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    post(
        [this, timeout = std::move(timeout), socketAttributes = std::move(socketAttributes),
            handler = std::move(handler)]() mutable
        {
            launchInactivityTimer();
            m_tunnelConnection->establishNewConnection(
                std::move(timeout),
                std::move(socketAttributes),
                std::move(handler));
        });
}

void OutgoingTunnelConnectionWatcher::launchInactivityTimer()
{
    if (m_connectionParameters.tunnelInactivityTimeout > std::chrono::seconds::zero())
    {
        m_inactivityTimer->cancelSync();
        m_inactivityTimer->start(
            m_connectionParameters.tunnelInactivityTimeout,
            std::bind(&OutgoingTunnelConnectionWatcher::closeTunnel, this,
                SystemError::timedOut));
    }
}

void OutgoingTunnelConnectionWatcher::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    using namespace std::placeholders;
    m_onTunnelClosedHandler = std::move(handler);
    m_tunnelConnection->setControlConnectionClosedHandler(
        std::bind(&OutgoingTunnelConnectionWatcher::closeTunnel, this, _1));
}

std::string OutgoingTunnelConnectionWatcher::toString() const
{
    return m_tunnelConnection
        ? m_tunnelConnection->toString()
        : std::string();
}

void OutgoingTunnelConnectionWatcher::closeTunnel(SystemError::ErrorCode reason)
{
    NX_ASSERT(isInSelfAioThread());

    m_inactivityTimer.reset();
    m_statusCode = reason;

    decltype(m_tunnelConnection) tunnelConnection;
    tunnelConnection.swap(m_tunnelConnection);
    
    decltype(m_onTunnelClosedHandler) onTunnelClosedHandler;
    onTunnelClosedHandler.swap(m_onTunnelClosedHandler);

    tunnelConnection.reset();

    if (onTunnelClosedHandler)
        onTunnelClosedHandler(reason);
}

} // namespace cloud
} // namespace network
} // namespace nx
