#pragma once

#include <memory>

#include <utils/common/long_runnable.h>

#include "abstract_pollset.h"
#include "aio_event_handler.h"
#include "event_type.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {

namespace detail {

class AioThreadImpl;

} // namespace detail

class NX_NETWORK_API AbstractAioThread
{
public:
    virtual ~AbstractAioThread() = default;
};

/**
 * This class implements socket event loop using PollSet class to do actual polling.
 * Also supports: 
 *   - Asynchronous functor execution via AIOThread::post or AIOThread::dispatch. 
 *   - Maximum timeout to wait for desired event.
 */
class NX_NETWORK_API AIOThread:
    public AbstractAioThread,
    public QnLongRunnable
{
public:
    /**
     * @param pollSet If null, will be created using PollSetFactory.
     */
    AIOThread(std::unique_ptr<AbstractPollSet> pollSet = nullptr);
    virtual ~AIOThread();

    virtual void pleaseStop();
    /**
     * Start monitoring socket sock for event eventToWatch and trigger eventHandler when event happens.
     * @note MUST be called with mutex locked.
     */
    void watchSocket(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>() );
    /**
     * Change timeout of existing polling sock for eventToWatch to timeoutMS. 
     *   eventHandler is changed also.
     * @note If sock is not polled, undefined behaviour can occur
     */
    void changeSocketTimeout(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0),
        std::function<void()> socketAddedToPollHandler = std::function<void()>() );
    /**
     * Stop monitorong sock for event eventType.
     * Garantees that no eventTriggered will be called after return of this method.
     * If eventTriggered is running and removeFromWatch called not from eventTriggered, 
     *   method blocks till eventTriggered had returned.
     * @param waitForRunningHandlerCompletion See comment to aio::AIOService::removeFromWatch.
     * @note Calling this method with same parameters simultaneously from 
     *   multiple threads can cause undefined behavour.
     * @note MUST be called with mutex locked.
     */
    void removeFromWatch(
        Pollable* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler = nx::utils::MoveOnlyFunc<void()>());
    /**
     * Queues functor to be executed from within this aio thread as soon as possible.
     */
    void post( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor );
    /**
     * If called in this aio thread, then calls functor immediately, 
     *   otherwise queues functor in same way as aio::AIOThread::post does.
     */
    void dispatch( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor );
    /**
     * Cancels calls scheduled with aio::AIOThread::post and aio::AIOThread::dispatch.
     */
    void cancelPostedCalls( Pollable* const sock, bool waitForRunningHandlerCompletion );
    /**
     * Returns number of sockets handled by this object.
     */
    size_t socketsHandled() const;

protected:
    virtual void run();

private:
    detail::AioThreadImpl* m_impl;
};

} // namespace aio
} // namespace network
} // namespace nx
