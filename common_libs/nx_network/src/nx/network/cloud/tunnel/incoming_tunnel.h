#ifndef NX_CC_CLOUD_TUNNEL_H
#define NX_CC_CLOUD_TUNNEL_H

#include <memory>
#include <queue>

#include "nx/network/cloud/tunnel/tunnel.h"


namespace nx {
namespace network {
namespace cloud {

class IncomingTunnel
:
    public Tunnel
{
public:
    IncomingTunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    /** Accept new incoming connection on the tunnel */
    void accept(SocketHandler handler);
};

//!Stores cloud tunnels
class IncomingTunnelPool
    : public QnStoppableAsync
{
public:
    /** Creates new tunnel with connected \param connection */
    void addNewTunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    void accept(std::shared_ptr<StreamSocketOptions> options,
                Tunnel::SocketHandler handler);

    //! QnStoppableAsync::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

private:
    void acceptTunnel(const String& peerId);
    void removeTunnel(const String& peerId);

    struct SocketRequest
    {
        std::shared_ptr<StreamSocketOptions> options;
        Tunnel::SocketHandler handler;
    };

    void waitConnectingSocket(std::unique_ptr<AbstractStreamSocket> socket,
                              SocketRequest request);

    void indicateAcceptedSocket(std::unique_ptr<AbstractStreamSocket> socket,
                                SocketRequest request);

    QnMutex m_mutex;
    std::map<String, std::shared_ptr<IncomingTunnel>> m_pool;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    boost::optional<SocketRequest> m_acceptRequest;
    std::queue<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_H
