#include "cloud_tunnel.h"

#include <nx/network/socket_global.h>

namespace nx {
namespace cc {

void CloudTunnel::addAddress(const AddressEntry& address)
{
    // we already know this address
    if (m_impls.count(address))
        return;

    switch(address.type)
    {
        // TODO: create certain AbstractCloudTunnelImpl
    };
}

void CloudTunnel::connect(std::shared_ptr<StreamSocketOptions> options,
                          SocketHandler handler)
{
    // TODO: if there some opened AbstractCloudTunnelImpl delegate connect
    //       to it, otherwise try to open all underlaing AbstractCloudTunnelImpls
    //       and delegate connect to the first successfuly opend

    static_cast<void>(options);
    static_cast<void>(handler);
}

void CloudTunnel::accept(SocketHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    Q_ASSERT_X(!handler, Q_FUNC_INFO, "simultaneous accept on the same tunnel");
    m_acceptHandler = std::move(handler);
}

void CloudTunnel::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& impl : m_impls)
        impl.second->pleaseStop(barrier.fork());
}

CloudTunnelPool::CloudTunnelPool()
    : m_mediatorConnection(SocketGlobals::mediatorConnector().systemConnection())
{
    m_mediatorConnection->monitorConnectionRequest(
        [this](MediatorSystemConnection::ConnectionRequest request)
    {
        QnMutexLocker lock(&m_mutex);
        switch(request)
        {
            // TODO: create certain AbstractCloudTunnelImpl and add it to
            //       m_pool
        };
    });
}

std::shared_ptr<CloudTunnel> CloudTunnelPool::getTunnel(const String& peerId)
{
    std::shared_ptr<CloudTunnel> tunnel;
    std::function<void(std::shared_ptr<CloudTunnel>)> monitor;
    {
        QnMutexLocker lock(&m_mutex);
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
        QnMutexLocker lock(&m_mutex);
        m_monitor = handler;
        currentTunnels = m_pool;
    }

    for (const auto& tunnel : currentTunnels)
        handler(tunnel.second);
}

void CloudTunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel.second->pleaseStop(barrier.fork());
}

} // namespace cc
} // namespace nx
