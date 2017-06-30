
#pragma once

#include <nx/network/async_stoppable.h>

#include "nx/network/cloud/address_resolver.h"
#include "nx/network/socket_attributes_cache.h"
#include "nx/network/socket_common.h"


namespace nx {
namespace network {
namespace cloud {

/** Represents connection established using a nat traversal method (UDP hole punching, tcp hole punching, reverse connection).
    \note Calling party MUST guarantee that no methods of this class are used after 
        \a QnStoppableAsync::pleaseStop call. 
    \note It is allowed to free object right in \a QnStoppableAsync::pleaseStop completion handler
 */
class AbstractTunnelConnection
:
    public QnStoppableAsync
{
public:
    AbstractTunnelConnection(String remotePeerId)
        : m_remotePeerId(std::move(remotePeerId))
    {
    }

    const String& getRemotePeerId() const { return m_remotePeerId; }

    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*stillValid*/)> SocketHandler;

    /** Creates new connection to the target peer.
        \note Can return the only connection reporting \a stillValid as \a false 
            in case if real tunneling is not supported by implementation
        \note Implementation MUST guarantee that all handlers are invoked before 
            \a QnStoppableAsync::pleaseStop completion
     */
    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
    *  \note not all of the AbstractTunnelConnection can really accept connections */
    virtual void accept(SocketHandler handler) = 0;

protected:
    const String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
