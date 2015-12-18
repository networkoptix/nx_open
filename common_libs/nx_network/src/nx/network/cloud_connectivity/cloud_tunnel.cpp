#include "cloud_tunnel.h"

namespace nx {
namespace cc {

std::shared_ptr<CloudTunnel> CloudTunnelPool::getTunnel(const String& peerId)
{
    std::shared_ptr<CloudTunnel> tunnel;
    std::function<void(std::shared_ptr<CloudTunnel>)> monitor;
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_pool.find(peerId);
        if(it == m_pool.end())
            it = m_pool.emplace(peerId, std::make_shared<CloudTunnel>()).first;

        tunnel = it->second;
        monitor = m_monitor;
    }

    if (monitor)
        monitor(tunnel);

    return tunnel;
}

void CloudTunnelPool::monitorTunnels(
        std::function<void(std::shared_ptr<CloudTunnel>)> handler)
{
    std::map<String, std::shared_ptr<CloudTunnel>> currentTunnels;
    {
        QMutexLocker lock(&m_mutex);
        m_monitor = handler;
        currentTunnels = m_pool;
    }

    for (const auto& tunnel : currentTunnels)
        handler(tunnel.second);
}

} // namespace cc
} // namespace nx
