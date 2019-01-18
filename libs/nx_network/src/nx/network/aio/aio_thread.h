#pragma once

#include <atomic>
#include <memory>

#include <nx/utils/thread/thread.h>

#include "abstract_pollset.h"
#include "aio_event_handler.h"
#include "event_type.h"

namespace nx::network { class Pollable; }

namespace nx::network::aio {

namespace detail { class AioTaskQueue; }

class NX_NETWORK_API AbstractAioThread
{
public:
    virtual ~AbstractAioThread() = default;

    /**
     * Queues functor to be executed from within this aio thread as soon as possible.
     */
    virtual void post(Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor) = 0;

    /**
     * Cancels calls scheduled with aio::AioThread::post and aio::AioThread::dispatch.
     */
    virtual void cancelPostedCalls(Pollable* const sock) = 0;
};

/**
 * This class implements socket event loop using PollSet class to do actual polling.
 * Also supports:
 *   - Asynchronous functor execution via AioThread::post or AioThread::dispatch.
 *   - Maximum timeout to wait for desired event.
 */
class NX_NETWORK_API AioThread:
    public AbstractAioThread,
    public nx::utils::Thread
{
public:
    /**
     * @param pollSet If null, will be created using PollSetFactory.
     */
    AioThread(std::unique_ptr<AbstractPollSet> pollSet = nullptr);
    virtual ~AioThread();

    virtual void pleaseStop() override;

    virtual void post(
        Pollable* const sock,
        nx::utils::MoveOnlyFunc<void()> functor) override;

    virtual void cancelPostedCalls(Pollable* const sock) override;

    /**
     * Start monitoring socket sock for event eventToWatch and trigger eventHandler when event happens.
     */
    void startMonitoring(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler,
        boost::optional<std::chrono::milliseconds> timeoutMillis,
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler);

    /**
     * Stop monitoring sock for event eventType.
     * Guarantees that no AIOEventHandler::eventTriggered will be called after return of this method.
     * If AIOEventHandler::eventTriggered is running and stopMonitoring called not from AIOEventHandler::eventTriggered,
     *   method blocks untill AIOEventHandler::eventTriggered had returned.
     * NOTE: Calling this method with same parameters simultaneously from
     *   multiple threads can cause undefined behavour.
     */
    void stopMonitoring(Pollable* const sock, aio::EventType eventType);
    /**
     * If called in this aio thread, then calls functor immediately,
     *   otherwise queues functor in same way as aio::AioThread::post does.
     */
    void dispatch(Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor);
    /**
     * Returns number of sockets handled by this object.
     */
    size_t socketsHandled() const;
    bool isSocketBeingMonitored(Pollable* sock) const;

    const detail::AioTaskQueue& taskQueue() const;

protected:
    virtual void run() override;

private:
    std::unique_ptr<AbstractPollSet> m_pollSet;
    std::unique_ptr<detail::AioTaskQueue> m_taskQueue;
    std::atomic<int> m_processingPostedCalls{0};

    bool getSocketTimeout(
        Pollable* const sock,
        aio::EventType eventToWatch,
        std::chrono::milliseconds* timeout);

    void startMonitoringInternal(
        const QnMutexLockerBase& /*lock*/,
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>());

    void stopMonitoringInternal(
        QnMutexLockerBase* lock,
        Pollable* const sock,
        aio::EventType eventType);

    /**
     * Change timeout of existing polling sock for eventToWatch to timeoutMS.
     *   eventHandler is changed also.
     * NOTE: If sock is not polled, undefined behaviour can occur.
     */
    void changeSocketTimeout(
        const QnMutexLockerBase& /*lock*/,
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0),
        std::function<void()> socketAddedToPollHandler = std::function<void()>());

    void post(
        const QnMutexLockerBase& /*lock*/,
        Pollable* const sock,
        nx::utils::MoveOnlyFunc<void()> functor);
};

} // namespace nx::network::aio
