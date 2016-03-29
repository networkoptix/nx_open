#pragma once

#include <utils/common/stoppable.h>
#include <utils/common/systemerror.h>

#include <nx/network/abstract_socket.h>

namespace nx {
namespace network {
namespace cloud {

/**
 *  Represents incomming tunnel connection established using one of a nat
 *  traversal methods.
 */
class NX_NETWORK_API AbstractIncomingTunnelConnection
:
    public QnStoppableAsync
{
public:
    AbstractIncomingTunnelConnection(String connectionId);

    /**
     *  Accepts new connection from peer (like socket)
     *
     *  \note Not all of the AbstractTunnelConnection can really accept
     *      connections so that they just return it's prmary connection
     *  \note If handler returns some error, this connection can not be reused
     *      and shell be deleted.
     */
    virtual void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> handler) = 0;

protected:
    const String m_connectionId;
};

} // namespace cloud
} // namespace network
} // namespace nx
