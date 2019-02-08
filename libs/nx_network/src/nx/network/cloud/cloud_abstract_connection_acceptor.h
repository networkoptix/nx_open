#pragma once

#include <nx/network/abstract_stream_socket_acceptor.h>

namespace nx {
namespace network {
namespace cloud {

class AbstractConnectionAcceptor:
    public AbstractStreamSocketAcceptor
{
public:
    /**
     * @return Ready-to-use connection from internal listen queue.
     *   nullptr if no connections available.
     */
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
