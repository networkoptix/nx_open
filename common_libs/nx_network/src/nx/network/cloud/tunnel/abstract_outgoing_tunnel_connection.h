/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>

#include <utils/common/stoppable.h>

#include <nx/network/socket_attributes_cache.h>


namespace nx {
namespace network {
namespace cloud {

/** 
 * \note Class instance can be safely freed within connect handler
 */
class AbstractOutgoingTunnelConnection
:
    public QnStoppableAsync
{
public:
    /**
        @param stillValid If \a false, connection cannot be used anymore 
            (every subsequent \a AbstractOutgoingTunnelConnection::establishNewConnection call will fail)
    */
    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool stillValid)> OnNewConnectionHandler;

    virtual ~AbstractOutgoingTunnelConnection() {}

    /**
        @param timeout zero - no timeout
        \note Actual implementation MUST support connect request pipelining but 
            does not have to be neither thread-safe nor reenterable
    */
    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) = 0;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
