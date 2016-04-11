/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <utils/common/stoppable.h>

#include "nx/network/aio/abstract_pollable.h"
#include "abstract_outgoing_tunnel_connection.h"
#include "tunnel.h"


namespace nx {
namespace network {
namespace cloud {

/** Creates outgoing specialized AbstractTunnelConnection.
    \note Methods of this class are not thread-safe
 */
class NX_NETWORK_API AbstractTunnelConnector
:
    public aio::AbstractPollable
{
public:
    /** Helps to decide which method shall be used first */
    virtual int getPriority() const = 0;
    /** Establishes connection to the target host.
        It is allowed to pipeline this method calls. 
        But these calls MUST be synchronized by caller.
        \param timeout 0 means no timeout
        \param handler \a AbstractTunnelConnector can be safely freed within this handler
     */
    virtual void connect(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler) = 0;
    virtual const AddressEntry& targetPeerAddress() const = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
