/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel_pool.h"

#include <nx/utils/log/log.h>
#include <utils/common/cpp14.h>


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelPool::OutgoingTunnelPool()
:
    m_terminated(false),
    m_stopping(false)
{
}

OutgoingTunnelPool::~OutgoingTunnelPool()
{
    NX_ASSERT(m_terminated);
}

void OutgoingTunnelPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    //stopping all tunnels. Assuming no one calls establishNewConnection anymore
    QnMutexLocker lock(&m_mutex);
    m_stopping = true;
    nx::BarrierHandler tunnelsStoppedFuture(
        [this, completionHandler = move(completionHandler)]() mutable
        {
            tunnelsStopped(std::move(completionHandler));
        });
    for (const auto& tunnel: m_pool)
        tunnel.second->pleaseStop(tunnelsStoppedFuture.fork());
}

void OutgoingTunnelPool::establishNewConnection(
    const AddressEntry& targetHostAddress,
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OutgoingTunnel::NewConnectionHandler handler)
{
    QnMutexLocker lock(&m_mutex);

    const auto& tunnel = getTunnel(targetHostAddress);
    tunnel->establishNewConnection(
        std::move(timeout),
        std::move(socketAttributes),
        std::move(handler));
}

const std::unique_ptr<OutgoingTunnel>& OutgoingTunnelPool::getTunnel(
    const AddressEntry& targetHostAddress)
{
    const auto iterAndInsertionResult = m_pool.emplace(
        targetHostAddress.host.toString(),
        std::unique_ptr<OutgoingTunnel>());
    if (!iterAndInsertionResult.second)
        return iterAndInsertionResult.first->second;

    NX_LOGX(
        lm("Creating outgoing tunnel to host %1").
            arg(targetHostAddress.host.toString()),
        cl_logDEBUG1);

    auto tunnel = std::make_unique<OutgoingTunnel>(targetHostAddress);
    const auto tunnelIter = iterAndInsertionResult.first;
    tunnel->setStateHandler(
        [this, tunnelPtr = tunnel.get()](Tunnel::State state)
        {
            if (state != Tunnel::State::kClosed)
                return;
            //tunnel supports deleting in "tunnel closed" handler
            onTunnelClosed(tunnelPtr);
        });

    iterAndInsertionResult.first->second = std::move(tunnel);
    return iterAndInsertionResult.first->second;
}

void OutgoingTunnelPool::onTunnelClosed(OutgoingTunnel* tunnelPtr)
{
    QnMutexLocker lk(&m_mutex);
    TunnelDictionary::iterator tunnelIter = m_pool.end();
    for (auto it = m_pool.begin(); it != m_pool.end(); ++it)
    {
        if (it->second.get() == tunnelPtr)
        {
            tunnelIter = it;
            break;
        }
    }

    if (m_stopping)
        return; //tunnel is being cancelled?

    NX_LOGX(lm("Removing tunnel to host %1").arg(tunnelIter->first), cl_logDEBUG1);
    auto tunnel = std::move(tunnelIter->second);
    m_pool.erase(tunnelIter);
    //m_aioThreadBinder.post(
    //    [tunnel = std::move(tunnel)]() mutable { tunnel.reset(); });
    lk.unlock();
    tunnel.reset();
}

void OutgoingTunnelPool::tunnelsStopped(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]
        {
            m_terminated = true;
            m_aioThreadBinder.pleaseStopSync();
            completionHandler();
        });
}

} // namespace cloud
} // namespace network
} // namespace nx
