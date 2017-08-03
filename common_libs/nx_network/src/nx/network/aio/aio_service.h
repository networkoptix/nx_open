#pragma once

#include <atomic>
#include <memory>
#include <atomic>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>

#include "aio_event_handler.h"
#include "aio_thread.h"

namespace nx {
namespace network {
namespace aio {

//TODO #ak think about removing sockets dictionary and mutex (only in release, probably). some data from dictionary can be moved to socket

/**
 * Monitors multiple sockets for asynchronous events and triggers handler (AIOEventHandler) on event.
 * Holds multiple threads inside, handler triggered from random thread.
 *   Number of internal threads can be specified during initialization.
 *   If not, it is selected at run-time as number of logical CPUs.
 * @note Suggested use of this class: few add/remove, many notifications.
 * @note A specific socket always reveives all events within the same thread.
 * @note Currently, win32 implementation uses select 
 *   (with win32 extensions making event array traversal O(1)). 
 *   Linux implementation uses epoll, BSD - kqueue.
 * @note All methods are thread-safe.
 * @note All methods are non-blocking except AIOService::stopMonitoring
 *   (when called with waitForRunningHandlerCompletion set to true).
 */
class NX_NETWORK_API AIOService
{
public:
    /**
     * After object instantiation one must call AIOService::isInitialized to check whether instantiation was a success.
     * @param threadCount This is minimal thread count. 
     *   Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
     *   If zero, thread count is choosed automatically.
     */
    AIOService(unsigned int threadCount = 0);
    virtual ~AIOService();

    void pleaseStopSync();

    /**
     * @return true, if object has been successfully initialized.
     */
    bool isInitialized() const;

    /**
     * Monitor socket sock for event eventToWatch and trigger eventHandler on event.
     * @return true, if added successfully. If false, error can be read by SystemError::getLastOSErrorCode() function.
     * @note if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported.
     * @note If not called from aio thread sock can be added to event loop with some delay.
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
     * @note If waitForRunningHandlerCompletion is false events that are already posted to the queue can be called after return of this method.
     * @note If this method is called from asio thread, sock is processed in (e.g., from event handler associated with sock).
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
    bool isSocketBeingWatched(Pollable* sock) const;

    /**
     * Call handler from within aio thread sock is bound to.
     * @note Call will always be queued. I.e., if called from handler 
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
     * @note If called in aio thread, handler will be called from within this method, 
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
        bool waitForRunningHandlerCompletion = true );

private:
    struct MonitoringContext
    {
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero();

        MonitoringContext() = default;
        MonitoringContext(std::chrono::milliseconds timeout):
            timeout(timeout)
        {
        }
    };

    struct SocketAIOContext
    {
        using AIOThreadType = AIOThread;

        std::vector<std::unique_ptr<AIOThreadType>> aioThreadPool;
        std::map<std::pair<Pollable*, aio::EventType>, MonitoringContext> sockets;
    };

    SocketAIOContext m_systemSocketAIO;
    mutable QnMutex m_mutex;

    void initializeAioThreadPool(SocketAIOContext* aioCtx, unsigned int threadCount);

    bool getSocketTimeout(
        Pollable* const sock,
        aio::EventType eventToWatch,
        std::chrono::milliseconds* timeout);
    boost::optional<MonitoringContext> getSocketMonitoringContext(
        const QnMutexLockerBase& /*lock*/,
        Pollable* const sock,
        aio::EventType eventToWatch);
    AIOThread* AIOService::findLeastUsedAioThread() const;
};

} // namespace aio
} // namespace network
} // namespace nx
