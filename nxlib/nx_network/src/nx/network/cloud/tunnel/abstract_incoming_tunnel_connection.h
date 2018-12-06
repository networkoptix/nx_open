#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace cloud {

// TODO: #ak Inherit aio::BasicPollable.

/**
 * Represents incomming tunnel connection established using one of a nat traversal methods.
 */
class NX_NETWORK_API AbstractIncomingTunnelConnection:
    public aio::BasicPollable
{
public:
    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> AcceptHandler;

    /**
     *  Accepts new connection from peer (like socket)
     *
     *  NOTE: Not all of the AbstractTunnelConnection can really accept
     *      connections so that they just return it's prmary connection
     *  NOTE: If handler returns some error, this connection can not be reused
     *      and shell be deleted.
     */
    virtual void accept(AcceptHandler handler) = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
