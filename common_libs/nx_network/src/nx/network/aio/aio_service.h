#pragma once

#include <atomic>
#include <memory>
#include <atomic>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/move_only_func.h>

#include "aio_event_handler.h"
#include "aio_thread.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Monitors multiple sockets for asynchronous events and triggers handler (AIOEventHandler) on event.
 * Holds multiple threads inside, handler triggered from random thread.
 *   Number of internal threads can be specified during initialization.
 *   If not, it is selected at run-time as number of logical CPUs.
 * This class encapsulates aio thread pool and dispatches socket call to a corresponding thread.
 * NOTE: Suggested use of this class: few add/remove, many notifications.
 * NOTE: A specific socket always receives all events within the same thread.
 * NOTE: All methods are thread-safe.
 * NOTE: All methods are non-blocking except AIOService::stopMonitoring
 *   (when called with waitForRunningHandlerCompletion set to true).
 */
class NX_NETWORK_API AIOService
{
public:
    /**
     * After object instantiation one must call AIOService::isInitialized to check
     * whether instantiation was a success.
     */
    AIOService() = default;
    virtual ~AIOService();

    AIOService(const AIOService&) = delete;
    AIOService& operator=(const AIOService&) = delete;

    void pleaseStopSync();

    /**
     * @return true, if object has been successfully initialized.
     */
    bool initialize(unsigned int aioThreadPollSize = 0);

    /**
     * Monitor sock for event eventToWatch and trigger eventHandler on event.
     * @return true, if added successfully. If false, error can be read by SystemError::getLastOSErrorCode().
     * NOTE: if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported.
     * NOTE: If not called from aio thread sock can be added to event loop with some delay.
     */
    void startMonitoring(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler,
        boost::optional<std::chrono::milliseconds> timeoutMillis
            = boost::optional<std::chrono::milliseconds>(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>());

    /**
     * Cancel monitoring sock for event eventType.
     * @param waitForRunningHandlerCompletion true garantees that no aio::AIOEventHandler::eventTriggered will be called after return of this method.
     *   and all running handlers have returned. But this MAKES METHOD BLOCKING and, as a result, this MUST NOT be called from aio thread.
     *   It is strongly recommended to set this parameter to false.
     * NOTE: If waitForRunningHandlerCompletion is false events that are already posted to the queue can be called after return of this method.
     * NOTE: If this method is called from asio thread, sock is processed in (e.g., from event handler associated with sock).
     *   this method does not block and always works like waitForRunningHandlerCompletion has been set to true.
     */
    void stopMonitoring(
        Pollable* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion = true,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler = nx::utils::MoveOnlyFunc<void()>());

    /**
     * Register timeout, associated with socket sock.
     * eventHandler->eventTriggered(sock, aio::etTimedOut) will be called every
     * timeoutMillis milliseconds until cancelled with
     * aio::AIOService::stopMonitoring(sock, aio::etTimedOut ).
     */
    void registerTimer(
        Pollable* const sock,
        std::chrono::milliseconds timeoutMillis,
        AIOEventHandler* const eventHandler );

    /**
     * @returns true, if socket is still listened for state changes.
     */
    bool isSocketBeingMonitored(Pollable* sock);

    /**
     * Call handler from within aio thread sock is bound to.
     * NOTE: Call will always be queued. I.e., if called from handler
     *   running in aio thread, it will be called after handler has returned.
     * WARNING: Currently, there is no way to find out whether call
     *   has been posted or being executed currently.
     */
    void post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler);
    /**
     * Calls handler in random aio thread.
     */
    void post(nx::utils::MoveOnlyFunc<void()> handler);
    /**
     * Call handler from within aio thread sock is bound to.
     * NOTE: If called in aio thread, handler will be called from within this method,
     *   otherwise - queued like aio::AIOService::post does.
     */
    void dispatch(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler);
    aio::AIOThread* getSocketAioThread(Pollable* sock);
    AbstractAioThread* getRandomAioThread() const;
    AbstractAioThread* getCurrentAioThread() const;
    bool isInAnyAioThread() const;

    void bindSocketToAioThread(Pollable* sock, AbstractAioThread* aioThread);
    aio::AIOThread* bindSocketToAioThread(Pollable* const sock);

    void cancelPostedCalls(
        Pollable* const sock,
        bool waitForRunningHandlerCompletion = true);

private:
    std::vector<std::unique_ptr<AIOThread>> m_aioThreadPool;

    void initializeAioThreadPool(unsigned int threadCount);

    AIOThread* findLeastUsedAioThread() const;
};

} // namespace aio
} // namespace network
} // namespace nx
