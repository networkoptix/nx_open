#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "aio/basic_pollable.h"

namespace nx {
namespace network {

using ReverseConnectionCompletionHandler = 
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

class AbstractAcceptableReverseConnection
{
public:
    virtual ~AbstractAcceptableReverseConnection() = default;

    virtual void connectAsync(
        ReverseConnectionCompletionHandler handler) = 0;
    virtual void waitForConnectionToBeReadyAsync(
        ReverseConnectionCompletionHandler handler) = 0;
};

/**
 * Server-side implementation of "reverse connection" technique.
 * That means: server establishes connection(s) to the client and waits for client
 *   to begin using that connection. 
 * When client begins using connection, this class provides connection to the caller 
 *   by acceptAsync or getNextConnectionIfAny methods.
 *
 * Connection is ready to be accepted when it has triggered handler 
 *   passed to AbstractAcceptableReverseConnection::waitForConnectionToBeReadyAsync 
 *   with SystemError::noError code.
 * Always keeps specified number of connections alive.
 * @param AcceptableReverseConnection MUST implement 
 *   AbstractAcceptableReverseConnection and aio::BasicPollable.
 * NOTE: This class follows behavior of AbstractAcceptor, though not implements it.
 * NOTE: Does not implement accept timeout. If you need it, consider using aio::Timer.
 * NOTE: If AbstractAcceptableReverseConnection::waitForConnectionToBeReadyAsync reports error, 
 *   that error is passed to the scheduled acceptAsync call.
 */
template<typename AcceptableReverseConnection>
class ReverseConnectionAcceptor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using AcceptCompletionHandler = 
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, 
                std::unique_ptr<AcceptableReverseConnection>)>;
    
    using ConnectionFactoryFunc = 
        nx::utils::MoveOnlyFunc<std::unique_ptr<AcceptableReverseConnection>()>;

    /**
     * @param connectionFactory connectionFactory() must return 
     *   std::unique_ptr<AcceptableReverseConnection>.
     */
    ReverseConnectionAcceptor(ConnectionFactoryFunc connectionFactory):
        m_connectionFactory(std::move(connectionFactory))
    {
    }

    void setPreemptiveConnectionCount(std::size_t)
    {
        // TODO
    }

    void setReadyConnectionQueueSize(std::size_t)
    {
        // TODO
    }

    void start()
    {
        // TODO
    }

    void acceptAsync(AcceptCompletionHandler /*handler*/)
    {
        // TODO
    }

    void cancelIOSync()
    {
        // TODO
    }

    std::unique_ptr<AcceptableReverseConnection> getNextConnectionIfAny()
    {
        // TODO
        return nullptr;
    }

private:
    ConnectionFactoryFunc m_connectionFactory;
};

} // namespace network
} // namespace nx
