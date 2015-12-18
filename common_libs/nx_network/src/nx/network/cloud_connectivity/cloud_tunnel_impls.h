#ifndef NX_CC_CLOUD_TUNNEL_IMPLS_H
#define NX_CC_CLOUD_TUNNEL_IMPLS_H

#include "cloud_tunnel.h"

namespace nx {
namespace cc {

class UdtCloudTunnelImpl
        : AbstractCloudTunnelImpl
{
public:
    void open(const SocketAddress& address,
              std::shared_ptr<StreamSocketOptions> options,
              std::function<void(SystemError::ErrorCode)> handler) override
    {
        QnMutexLocker(&m_mutex);
        m_address = address;
        m_handler = std::move(handler);

        // TODO: initiates UDP hole punching and create m_tunnelSocket
    }

    void connect(std::shared_ptr<StreamSocketOptions> options,
                 SocketHandler handler) override
    {
        QnMutexLocker(&m_mutex);
        if (m_tunnelSocket)
        {
            // TODO: try to create another connection,
            //         return m_tunnelSocke and call m_connectionHandler otherwise
        }
        else
        {
            // TODO: wait for m_tunnelSocket to be estabilished
        }
    }

    void accept(SocketHandler handler) override
    {
        QnMutexLocker(&m_mutex);
        m_acceptHandler = std::move(handler);

        // TODO: m_tunnelSocket->read for new connection requests and call
        //       m_acceptHandler when new connection is estabilished
    }

    bool isOpen() override
    {
        QnMutexLocker(&m_mutex);
        return m_tunnelSocket;
    }

private:
    QnMutex m_mutex;
    SocketAddress m_address;
    std::function<void(SystemError::ErrorCode)> m_connectionHandler;
    std::function<void(SystemError::ErrorCode)> m_acceptHandler;
    std::unique_ptr<AbstractStreamSocket> m_tunnelSocket;
};

} // namespace cc
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_IMPLS_H
