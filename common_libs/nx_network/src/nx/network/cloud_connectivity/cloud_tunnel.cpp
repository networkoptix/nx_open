#include "cloud_tunnel.h"

#include <nx/network/socket_global.h>
#include <nx/network/cloud_connectivity/cloud_tunnel_udt.h>

namespace nx {
namespace cc {

Tunnel::Tunnel(String peerId, std::vector<CloudConnectType> types)
{
    for (const auto type : types)
    {
        std::unique_ptr<AbstractTunnelConnector> connector;
        switch(type)
        {
            case CloudConnectType::udtHp:
                connector = std::make_unique<UdtTunnelConnector>(peerId);

            default:
                // Probably just log the ERROR
                Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported CloudConnectType value!");
        };

        if (connector)
            m_connectors.emplace(connector->getPriority(), std::move(connector));
    }

    m_connectorsItreator = m_connectors.begin();
}

void Tunnel::connect(std::shared_ptr<StreamSocketOptions> options,
                     SocketHandler handler)
{
    QnMutexLocker lk(&m_mutex);

    // try to use existed connection if avaliable
    if (m_connection)
    {
        // TODO: m_connection.connect(options, [handler](...){...});
        return;
    }

    if (m_connectorsItreator == m_connectors.end())
    {
        lk.unlock();
        handler(SystemError::notConnected, std::unique_ptr<AbstractStreamSocket>());
        return;
    }

    auto currentPriority = m_connectorsItreator->first;
    do
    {
        // TODO: m_connectorsItreator->second->connect(options, [handler](...){...});

        if (++m_connectorsItreator == m_connectors.end())
            return;
    }
    while (currentPriority == m_connectorsItreator->first);
}

void Tunnel::accept(SocketHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    Q_ASSERT_X(!handler, Q_FUNC_INFO, "simultaneous accept on the same tunnel");
    m_acceptHandler = std::move(handler);
}

void Tunnel::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& connector : m_connectors)
        connector.second->pleaseStop(barrier.fork());

    if (m_connection)
        m_connection->pleaseStop(barrier.fork());
}

TunnelPool::TunnelPool()
    : m_mediatorConnection(SocketGlobals::mediatorConnector().systemConnection())
{
    m_mediatorConnection->monitorConnectionRequest(
        [this](MediatorSystemConnection::ConnectionRequest request)
    {
        QnMutexLocker lock(&m_mutex);
        switch(request)
        {
            // TODO: create certain AbstractAbstractTunnelConnection and add it to
            //       m_pool
        };
    });
}

std::shared_ptr<Tunnel> TunnelPool::get(
        const String& peerId, std::vector<CloudConnectType> connectTypes)
{
    std::shared_ptr<Tunnel> tunnel;
    std::function<void(std::shared_ptr<Tunnel>)> monitor;
    {
        QnMutexLocker lock(&m_mutex);
        auto it = m_pool.find(peerId);
        if(it == m_pool.end())
        {
            auto tunnel = std::make_shared<Tunnel>(peerId, std::move(connectTypes));
            it = m_pool.emplace(peerId, std::move(tunnel)).first;
        }

        tunnel = it->second;
        monitor = m_monitor;
    }

    if (monitor)
        monitor(tunnel);

    return tunnel;
}

void TunnelPool::setNewHandler(
        std::function<void(std::shared_ptr<Tunnel>)> handler)
{
    std::map<String, std::shared_ptr<Tunnel>> currentTunnels;
    {
        QnMutexLocker lock(&m_mutex);
        m_monitor = handler;
        currentTunnels = m_pool;
    }

    for (const auto& tunnel : currentTunnels)
        handler(tunnel.second);
}

void TunnelPool::pleaseStop(std::function<void()> handler)
{
    BarrierHandler barrier(std::move(handler));

    QnMutexLocker lock(&m_mutex);
    for (const auto& tunnel : m_pool)
        tunnel.second->pleaseStop(barrier.fork());
}

} // namespace cc
} // namespace nx
