/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOSERVICE_H
#define AIOSERVICE_H

#include <atomic>
#include <memory>
#include <atomic>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>

#include "aioeventhandler.h"
#include "aiothread.h"


namespace nx {
namespace network {

class SocketGlobals;

namespace aio {

//TODO #ak think about removing sockets dictionary and mutex (only in release, probably). some data from dictionary can be moved to socket

//!Monitors multiple sockets for asynchronous events and triggers handler (\a AIOEventHandler) on event
/*!
    Holds multiple threads inside, handler triggered from random thread
    \note Internal thread count can be increased dynamically, since this class uses PollSet class which can monitor only limited number of sockets
    \note Suggested use of this class: little add/remove, many notifications
    \note Single socket is always listened for all requested events by the same thread
    \note Currently, win32 implementation uses select, so it is far from being efficient. Linux implementation uses epoll, BSD - kqueue
    \note All methods are thread-safe
    \note All methods are not blocking except \a AIOService::removeFromWatch called with \a waitForRunningHandlerCompletion set to \a true
*/
class NX_NETWORK_API AIOService
{
    /*!
        After object instanciation one must call \a isInitialized to check whether instanciation was a success
        \param threadCount This is minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
        If zero, thread count is choosed automatically
    */
    AIOService( unsigned int threadCount = 0 );
    virtual ~AIOService();
    friend class ::nx::network::SocketGlobals;

public:
    //!Returns true, if object has been successfully initialized
    bool isInitialized() const;

    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
        \note if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported
        \note If not called from aio thread \a sock can be added to event loop with some delay
    */
    void watchSocket(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>());

    //!Cancel monitoring \a sock for event \a eventType
    /*!
        \param waitForRunningHandlerCompletion \a true garantees that no \a aio::AIOEventHandler::eventTriggered will be called after return of this method
        and all running handlers have returned. But this MAKES METHOD BLOCKING and, as a result, this MUST NOT be called from aio thread
        It is strongly recommended to set this parameter to \a false.

        \note If \a waitForRunningHandlerCompletion is \a false events that are already posted to the queue can be called \b after return of this method
        \note If this method is called from asio thread, \a sock is processed in (e.g., from event handler associated with \a sock),
        this method does not block and always works like \a waitForRunningHandlerCompletion has been set to \a true
    */
    void removeFromWatch(
        Pollable* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion = true,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler = nx::utils::MoveOnlyFunc<void()>());

    //!Register timeout, associated with socket \a sock
    /*!
        \a eventHandler->eventTriggered( \a sock, aio::etTimedOut ) will be called every \a timeoutMillis milliseconds until cancelled with \a aio::AIOService::removeFromWatch( \a sock, \a aio::etTimedOut )
    */
    void registerTimer(
        Pollable* const sock,
        std::chrono::milliseconds timeoutMillis,
        AIOEventHandler<Pollable>* const eventHandler );

    //!Returns \a true, if socket is still listened for state changes
    bool isSocketBeingWatched(Pollable* sock) const;

    //!Call \a handler from within aio thread \a sock is bound to
    /*!
        \note Call will always be queued. I.e., if called from handler running in aio thread, it will be called after handler has returned
        \warning Currently, there is no way to find out whether call has been posted or being executed currently
    */
    void post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler);
    //!Calls \a handler in random aio thread
    void post(nx::utils::MoveOnlyFunc<void()> handler);
    //!Call \a handler from within aio thread \a sock is bound to
    /*!
        \note If called in aio thread, handler will be called from within this method, otherwise - queued like \a aio::AIOService::post does
    */
    void dispatch(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler);
    //TODO #ak better remove this method, violates encapsulation
    QnMutex* mutex() const;
    aio::AIOThread* getSocketAioThread(Pollable* sock);
    AbstractAioThread* getRandomAioThread() const;
    void bindSocketToAioThread(Pollable* sock, AbstractAioThread* aioThread);

    //!Same as \a AIOService::watchSocket, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
    /*!
        \param socketAddedToPollHandler Called after socket has been added to pollset but before pollset.poll has been called
    */
    void watchSocketNonSafe(
        QnMutexLockerBase* const lock,
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        boost::optional<std::chrono::milliseconds> timeoutMillis
            = boost::optional<std::chrono::milliseconds>(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler
            = nx::utils::MoveOnlyFunc<void()>());

    //!Same as \a AIOService::removeFromWatch, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
    void removeFromWatchNonSafe(
        QnMutexLockerBase* const lock,
        Pollable* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion = true,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler
            = nx::utils::MoveOnlyFunc<void()>());

    void cancelPostedCalls(
        Pollable* const sock,
        bool waitForRunningHandlerCompletion = true );

private:
    struct SocketAIOContext
    {
        typedef AIOThread AIOThreadType;

        std::vector<std::unique_ptr<AIOThreadType>> aioThreadPool;
        //!map<pair<socket, event_type>, pair<thread, socket timeout>>
        std::map<
            std::pair<Pollable*, aio::EventType>,
            std::pair<AIOThreadType*, std::chrono::milliseconds> > sockets;
    };

    SocketAIOContext m_systemSocketAIO;
    mutable QnMutex m_mutex;

    void initializeAioThreadPool(SocketAIOContext* aioCtx, unsigned int threadCount);
    aio::AIOThread* getSocketAioThread(
        QnMutexLockerBase* const lock,
        Pollable* sock);
    void cancelPostedCallsNonSafe(
        QnMutexLockerBase* const lock,
        Pollable* const sock,
        bool waitForRunningHandlerCompletion = true );
    aio::AIOThread* bindSocketToAioThread(
        QnMutexLockerBase* const /*lock*/,
        Pollable* const sock );
    void postNonSafe(
        QnMutexLockerBase* const lock,
        Pollable* sock,
        nx::utils::MoveOnlyFunc<void()> handler);
};

}   //aio
}   //network
}   //nx

#endif  //AIOSERVICE_H
