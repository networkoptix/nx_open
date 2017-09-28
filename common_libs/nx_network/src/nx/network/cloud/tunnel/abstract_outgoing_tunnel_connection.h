#pragma once

#include <chrono>
#include <string>

#include <nx/network/async_stoppable.h>

#include "nx/network/aio/basic_pollable.h"
#include "nx/network/socket_attributes_cache.h"

namespace nx {
namespace network {
namespace cloud {

/** 
 * Cross-Nat tunnel on connector side.
 * @note Class instance can be safely freed within connect handler.
 */
class AbstractOutgoingTunnelConnection:
    public aio::BasicPollable
{
public:
    /**
     * @param stillValid If false, connection cannot be used anymore 
     * (every subsequent AbstractOutgoingTunnelConnection::establishNewConnection call will fail)
     */
    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool stillValid)> OnNewConnectionHandler;

    AbstractOutgoingTunnelConnection(aio::AbstractAioThread* aioThread = nullptr):
        aio::BasicPollable(aioThread)
    {
    }
    virtual ~AbstractOutgoingTunnelConnection() {}

    /**
     * Start monitoring connection.
     * This method is needed to ensure that caller has subscribed to events before latter arrive.
     */
    virtual void start() = 0;

    /**
     * @param timeout zero - no timeout
     * @note Actual implementation MUST support connect request pipelining but 
     * does not have to be neither thread-safe nor reenterable
     */
    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) = 0;

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    virtual std::string toString() const = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
