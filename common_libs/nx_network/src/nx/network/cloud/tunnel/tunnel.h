#pragma once

#include <nx/network/async_stoppable.h>

#include "nx/network/address_resolver.h"
#include "nx/network/socket_attributes_cache.h"
#include "nx/network/socket_common.h"
#include "tunnel_attributes.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Represents connection established using a nat traversal method (UDP hole punching, tcp hole punching, reverse connection).
 * NOTE: Calling party MUST guarantee that no methods of this class are used after
 * QnStoppableAsync::pleaseStop call.
 * NOTE: It is allowed to free object right in QnStoppableAsync::pleaseStop completion handler
 */
class AbstractTunnelConnection:
    public QnStoppableAsync
{
public:
    AbstractTunnelConnection(String remotePeerId):
        m_remotePeerId(std::move(remotePeerId))
    {
    }

    const String& getRemotePeerId() const { return m_remotePeerId; }

    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*stillValid*/)> SocketHandler;

    /**
     * Creates new connection to the target peer.
     * NOTE: Can return the only connection reporting stillValid as false
     *   in case if real tunneling is not supported by implementation
     * NOTE: Implementation MUST guarantee that all handlers are invoked before
     *  QnStoppableAsync::pleaseStop completion
     */
    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler) = 0;

    /**
     * Accepts new connection from peer (like socket).
     * NOTE: not all of the AbstractTunnelConnection can really accept connections.
     */
    virtual void accept(SocketHandler handler) = 0;

protected:
    const String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
