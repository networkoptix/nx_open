/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel_pool.h"

#include <nx/utils/log/log.h>


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelPool::~OutgoingTunnelPool()
{
    //TODO #ak killing all tunnels
}

void OutgoingTunnelPool::pleaseStop(std::function<void()> completionHandler)
{
    //TODO #ak
}

std::shared_ptr<OutgoingTunnel> OutgoingTunnelPool::getTunnel(
    const AddressEntry& targetHostAddress)
{
    QnMutexLocker lock(&m_mutex);

    const auto iterAndInsertionResult = m_pool.emplace(
        targetHostAddress.host.toString(),
        std::shared_ptr<OutgoingTunnel>());
    if (!iterAndInsertionResult.second)
        return iterAndInsertionResult.first->second;

    NX_LOG(
        lm("Creating outgoing tunnel to host %1").
            arg(targetHostAddress.host.toString()),
        cl_logDEBUG1);

    const auto tunnel = std::make_shared<OutgoingTunnel>(targetHostAddress);
    const auto tunnelIter = iterAndInsertionResult.first;
    tunnel->setStateHandler(
        [this, tunnelIter](Tunnel::State state)
    {
        if (state != Tunnel::State::kClosed)
            return;
        removeTunnel(tunnelIter);
    });

    iterAndInsertionResult.first->second = tunnel;
    return tunnel;
}

void OutgoingTunnelPool::removeTunnel(TunnelDictionary::iterator tunnelIter)
{
    QnMutexLocker lock(&m_mutex);
    NX_LOG(lm("Removing tunnel to host %1").arg(tunnelIter->first), cl_logDEBUG1);
    m_pool.erase(tunnelIter);
}

} // namespace cloud
} // namespace network
} // namespace nx
