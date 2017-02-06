#pragma once

#include <deque>
#include <set>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/timer.h>
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
    IncomingTunnelPool(aio::AbstractAioThread* aioThread, size_t acceptLimit);

    /** Saves and accepts \param connection until it's exhausted */
    void addNewTunnel(std::unique_ptr<AbstractIncomingTunnelConnection> connection);

    /** Returns queued socket if avaliable, returns nullptr otherwise */
    std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny();

    /** Calls @param handler as soon as any socket is avaliable */
    void getNextSocketAsync(
        nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler,
        boost::optional<unsigned int> timeout);
    /** Cancels \a IncomingTunnelPool::getNextSocketAsync call.
        \note Does not block
    */
    void cancelAccept();

    /** Cancels all operations in progress */
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    using TunnelPool = std::set<std::unique_ptr<AbstractIncomingTunnelConnection>>;
    using TunnelIterator = TunnelPool::iterator;

    void acceptTunnel(TunnelIterator connection);
    void callAcceptHandler(bool timeout);

    const size_t m_acceptLimit;
    mutable QnMutex m_mutex;
    TunnelPool m_pool;
    aio::Timer m_aioTimer;
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> m_acceptHandler;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
};

} // namespace cloud
} // namespace network
} // namespace nx
