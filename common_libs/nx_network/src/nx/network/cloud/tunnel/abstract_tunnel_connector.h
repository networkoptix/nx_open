/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <utils/common/stoppable.h>

#include "tunnel.h"


namespace nx {
namespace network {
namespace cloud {

/** Creates outgoing specialized AbstractTunnelConnection.
    \note Methods of this class are not thread-safe
 */
class AbstractTunnelConnector
:
    public QnStoppableAsync
{
public:
    /** Helps to decide which method shall be used first */
    virtual int getPriority() const = 0;
    /** Establishes connection to the target host.
        It is allowed to pipeline this method calls. 
        But these calls MUST be synchronized by caller.
        \param handler \a AbstractTunnelConnector can be safely freed within this handler
     */
    virtual void connect(
        std::chrono::milliseconds timeout,
        std::function<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractTunnelConnection>)> handler) = 0;
    virtual const AddressEntry& targetPeerAddress() const = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
