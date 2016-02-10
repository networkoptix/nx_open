#ifndef NX_CC_CLOUD_TUNNEL_H
#define NX_CC_CLOUD_TUNNEL_H

#include <memory>
#include <deque>

#include "nx/network/cloud/tunnel/tunnel.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API IncomingTunnel
:
    public Tunnel
{
public:
    IncomingTunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    /** Accept new incoming connection on the tunnel */
    void accept(SocketHandler handler);
};

/** Stores incoming cloud tunnels and accepts new sockets from them */
class NX_NETWORK_API IncomingTunnelPool
    : public QnStoppableAsync
{
public:
    IncomingTunnelPool(size_t acceptLimit);

    /** Creates new tunnel with connected \param connection */
    void addNewTunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    /** Returns queued socket if avaliable, returns nullptr otherwise */
    std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny();

    /** Calls @param handler as soon as any socket is avaliable */
    void getNextSocketAsync(
        std::function<void(std::unique_ptr<AbstractStreamSocket>)> handler,
        boost::optional<unsigned int> timeout);

    /** Cancels all operations in progress */
    void pleaseStop(std::function<void()> handler) override;

private:
    void acceptTunnel(std::shared_ptr<IncomingTunnel> tunnel);
    void callAcceptHandler(bool timeout);

    const size_t m_acceptLimit;
    mutable QnMutex m_mutex;

    std::set<std::shared_ptr<IncomingTunnel>> m_pool;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    std::function<void(std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_H
