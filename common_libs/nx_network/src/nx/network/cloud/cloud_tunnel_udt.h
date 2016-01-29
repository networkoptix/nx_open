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
    UdtTunnelConnector(String targetId)
    {
        // TODO: save ids and run UdtTunnelConnection
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
};

class UdtTunnelAcceptor
        : public AbstractTunnelAcceptor
{
public:
    UdtTunnelAcceptor(String connectionId, String selfId, String targetId)
    {
        // TODO: save ids and run UdtTunnelConnection
    }

    void setTargetAddresses(std::list<SocketAddress> addresses)
    {
        // Save addresses
        static_cast<void>(addresses);
    }

    void accept(
        std::function<void(std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        // TODO: listen for mediator indcations and initiate UdtTunnelConnection
    }

    void pleaseStop(std::function<void()> handler) override
    {
        // TODO: cancel all async operations
    }

    const String m_connectionId;
    const String m_selfId;
    const String m_targetId;
};

class UdtTunnelConnection
        : public AbstractTunnelConnection
{
public:
    UdtTunnelConnection(String peerId_, SocketAddress address_)
        : AbstractTunnelConnection(peerId_)
    {
        // TODO: create m_tunnelSocket and open randevous connection
    }

    void connect(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
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
