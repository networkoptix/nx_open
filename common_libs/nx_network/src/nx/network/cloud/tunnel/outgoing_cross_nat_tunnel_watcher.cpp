#include "outgoing_cross_nat_tunnel_watcher.h"

#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {

OutgoingCrossNatTunnelWatcher::OutgoingCrossNatTunnelWatcher(
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

OutgoingCrossNatTunnelWatcher::~OutgoingCrossNatTunnelWatcher()
{
    stopWhileInAioThread();
}

void OutgoingCrossNatTunnelWatcher::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_tunnelConnection->bindToAioThread(getAioThread());
    m_inactivityTimer->bindToAioThread(getAioThread());
}

void OutgoingCrossNatTunnelWatcher::stopWhileInAioThread()
{
    m_tunnelConnection.reset();
    m_inactivityTimer.reset();
}

void OutgoingCrossNatTunnelWatcher::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    launchInactivityTimer();

    m_tunnelConnection->establishNewConnection(
        timeout,
        std::move(socketAttributes),
        std::move(handler));
}

void OutgoingCrossNatTunnelWatcher::launchInactivityTimer()
{
    if (m_connectionParameters.crossNatTunnelInactivityTimeout > std::chrono::seconds::zero())
    {
        m_inactivityTimer->cancelSync();
        m_inactivityTimer->start(
            m_connectionParameters.crossNatTunnelInactivityTimeout,
            std::bind(&OutgoingCrossNatTunnelWatcher::onInactivityTimoutExpired, this));
    }
}

void OutgoingCrossNatTunnelWatcher::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    using namespace std::placeholders;

    m_onTunnelClosedHandler = std::move(handler);
    m_tunnelConnection->setControlConnectionClosedHandler(
        std::bind(&OutgoingCrossNatTunnelWatcher::onTunnelClosed, this, _1));
}

void OutgoingCrossNatTunnelWatcher::onInactivityTimoutExpired()
{
    onTunnelClosed(SystemError::timedOut);
    m_tunnelConnection.reset();
}

void OutgoingCrossNatTunnelWatcher::onTunnelClosed(SystemError::ErrorCode reason)
{
    NX_ASSERT(isInSelfAioThread());

    m_inactivityTimer.reset();
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
