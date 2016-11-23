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
    launchInactivityTimer();
}

OutgoingTunnelConnectionWatcher::~OutgoingTunnelConnectionWatcher()
{
    stopWhileInAioThread();
}

void OutgoingTunnelConnectionWatcher::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_tunnelConnection->bindToAioThread(getAioThread());
    m_inactivityTimer->bindToAioThread(getAioThread());
}

void OutgoingTunnelConnectionWatcher::stopWhileInAioThread()
{
    m_tunnelConnection.reset();
    m_inactivityTimer.reset();
}

void OutgoingTunnelConnectionWatcher::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    post(
        [this, timeout = std::move(timeout), socketAttributes = std::move(socketAttributes),
            handler = std::move(handler)]()
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

void OutgoingTunnelConnectionWatcher::closeTunnel(SystemError::ErrorCode reason)
{
    NX_ASSERT(isInSelfAioThread());

    m_inactivityTimer.reset();
    m_tunnelConnection.reset();
    if (m_onTunnelClosedHandler)
    {
        auto handler = std::move(m_onTunnelClosedHandler);
        m_onTunnelClosedHandler = nullptr;
        handler(reason);
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
