#pragma once

#include <nx/network/abstract_socket.h>
#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

/**
 *  Stores incoming cloud tunnels and accepts new sockets from them
 */
class NX_NETWORK_API IncomingTunnelPool
    : public QnStoppableAsync
{
public:
    IncomingTunnelPool(aio::AbstractAioThread* ioThread, size_t acceptLimit);

    /** Saves and accepts \param connection until it's exhausted */
    void addNewTunnel(std::unique_ptr<AbstractIncomingTunnelConnection> connection);

    /** Returns queued socket if avaliable, returns nullptr otherwise */
    std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny();

    /** Calls @param handler as soon as any socket is avaliable */
    void getNextSocketAsync(
        std::function<void(std::unique_ptr<AbstractStreamSocket>)> handler,
        boost::optional<unsigned int> timeout);

    /** Cancels all operations in progress */
    void pleaseStop(std::function<void()> handler) override;

private:
    void acceptTunnel(std::shared_ptr<AbstractIncomingTunnelConnection> connection);
    void callAcceptHandler(bool timeout);

    const size_t m_acceptLimit;
    mutable QnMutex m_mutex;

    std::set<std::shared_ptr<AbstractIncomingTunnelConnection>> m_pool;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    std::function<void(std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
};

} // namespace cloud
} // namespace network
} // namespace nx
