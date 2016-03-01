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
    m_terminated(false)
{
}

OutgoingTunnelPool::~OutgoingTunnelPool()
{
    NX_ASSERT(m_terminated);
}

void OutgoingTunnelPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    //stopping all tunnels. Assuming noone calls establishNewConnection anymore
    QnMutexLocker lock(&m_mutex);
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
        [this, tunnelIter](Tunnel::State state)
        {
            if (state != Tunnel::State::kClosed)
                return;
            //tunnel supports deleting in "tunnel closed" handler
            removeTunnel(tunnelIter);
        });

    iterAndInsertionResult.first->second = std::move(tunnel);
    return iterAndInsertionResult.first->second;
}

void OutgoingTunnelPool::removeTunnel(TunnelDictionary::iterator tunnelIter)
{
    QnMutexLocker lock(&m_mutex);
    NX_LOGX(lm("Removing tunnel to host %1").arg(tunnelIter->first), cl_logDEBUG1);
    m_pool.erase(tunnelIter);
}

void OutgoingTunnelPool::tunnelsStopped(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_terminated = true;
    completionHandler();
}

} // namespace cloud
} // namespace network
} // namespace nx
