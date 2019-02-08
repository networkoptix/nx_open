#pragma once

#include <chrono>
#include <deque>
#include <set>

#include <boost/optional.hpp>

#include <nx/network/abstract_stream_socket_acceptor.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/aio/timer.h>

#include "abstract_tunnel_acceptor.h"
#include "../cloud_abstract_connection_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Stores incoming cloud tunnels and accepts new sockets from them.
 */
class NX_NETWORK_API IncomingTunnelPool:
    public AbstractConnectionAcceptor
{
    using base_type = AbstractStreamSocketAcceptor;

public:
    IncomingTunnelPool(aio::AbstractAioThread* aioThread, size_t acceptLimit);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

    void setAcceptTimeout(boost::optional<std::chrono::milliseconds> acceptTimeout);

    /** Saves and accepts @param connection until it's exhausted. */
    void addNewTunnel(std::unique_ptr<AbstractIncomingTunnelConnection> connection);

private:
    using TunnelPool = std::set<std::unique_ptr<AbstractIncomingTunnelConnection>>;
    using TunnelIterator = TunnelPool::iterator;

    const size_t m_acceptLimit;
    mutable QnMutex m_mutex;
    TunnelPool m_pool;
    aio::Timer m_timer;
    AcceptCompletionHandler m_acceptHandler;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
    boost::optional<std::chrono::milliseconds> m_acceptTimeout;

    virtual void stopWhileInAioThread() override;

    void acceptTunnel(TunnelIterator connection);
    void callAcceptHandler(SystemError::ErrorCode resultCode);
};

} // namespace cloud
} // namespace network
} // namespace nx
