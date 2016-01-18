#ifndef NX_CC_CLOUD_TUNNEL_IMPLS_H
#define NX_CC_CLOUD_TUNNEL_IMPLS_H

#include "cloud_tunnel.h"

namespace nx {
namespace network {
namespace cloud {

class UdtTunnelConnector
        : public AbstractTunnelConnector
{
public:
    UdtTunnelConnector(String peerId_)
        : peerId(std::move(peerId_))
    {
    }

    uint32_t getPriority() override
    {
        return 0; // maximum priority
    }

    void connect(
        std::function<void(std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        // TODO: initiate UDP hole punching to resolve SocketAddress by peerId_
        //       and create UdtTunnelConnection
    }

    void pleaseStop(std::function<void()> handler) override
    {
        // TODO: cancel all async operations
    }

private:
    const String peerId;
};

class UdtTunnelAcceptor
        : public AbstractTunnelAcceptor
{
public:
    UdtTunnelAcceptor()
    {
    }

    void accept(
        std::function<void(String,
                           std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        // TODO: listen for mediator indcations and initiate UdtTunnelConnection
    }

    void pleaseStop(std::function<void()> handler) override
    {
        // TODO: cancel all async operations
    }
};

class UdtTunnelConnection
        : public AbstractTunnelConnection
{
public:
    UdtTunnelConnection(String peerId_, SocketAddress address_)
        : peerId(peerId_)
        , address(address_)
    {
        // TODO: create m_tunnelSocket and open randevous connection
    }

    void connect(std::chrono::milliseconds timeout,
                 SocketHandler handler) override
    {
        QnMutexLocker lk(&m_mutex);
        if (m_tunnelSocket)
        {
            // TODO: try to create another connection,
            //         return m_tunnelSocke and call m_connectionHandler otherwise
        }
        else
        {
            m_connectHandlers.push_back(std::move(handler));
        }
    }

    void accept(SocketHandler handler) override
    {
        QnMutexLocker lk(&m_mutex);
        m_acceptHandler = std::move(handler);

        // TODO: m_tunnelSocket->read for new connection requests and call
        //       m_acceptHandler when new connection is estabilished
    }

    void pleaseStop(std::function<void()> handler) override
    {
        // TODO: cancel all async oiperations
    }

private:
    const String peerId;
    const SocketAddress address;

    QnMutex m_mutex;
    SocketAddress m_address;
    std::vector<SocketHandler> m_connectHandlers;
    SocketHandler m_acceptHandler;
    std::unique_ptr<AbstractStreamSocket> m_tunnelSocket;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_IMPLS_H
