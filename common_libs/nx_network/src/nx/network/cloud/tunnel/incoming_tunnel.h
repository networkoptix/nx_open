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

    virtual void pleaseStop(std::function<void()> completionHandler) override;

    /** Accept new incoming connection on the tunnel */
    void accept(SocketHandler handler);
};

/** Stores incoming cloud tunnels and accepts new sockets from them */
class IncomingTunnelPool
    : public QnStoppableAsync
{
public:
    /** Creates new tunnel with connected \param connection */
    void addNewTunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    /** Accepts new client socket as soon as some avaliable from on tunnel
     *  in this pool */
    void acceptNewSocket(Tunnel::SocketHandler handler);

    /** Cancels all operations in progress */
    void pleaseStop(std::function<void()> handler) override;

private:
    void acceptTunnel(std::shared_ptr<IncomingTunnel> tunnel);
    void indicateFirstSocket(QnMutexLockerBase* lock);

    QnMutex m_mutex;
    std::set<std::shared_ptr<IncomingTunnel>> m_pool;
    boost::optional<Tunnel::SocketHandler> m_acceptRequest;
    std::queue<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
    std::unique_ptr<AbstractStreamSocket> m_indicatingSocket;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_H
