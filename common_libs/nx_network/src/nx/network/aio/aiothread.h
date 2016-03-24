/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOTHREAD_H
#define AIOTHREAD_H

#include <utils/common/long_runnable.h>

#include "aioeventhandler.h"
#include "event_type.h"


namespace nx {
namespace network {

class Pollable;

namespace aio {

namespace detail {
class AIOThreadImpl;
}   //detail

class NX_NETWORK_API AbstractAioThread
{
public:
    virtual ~AbstractAioThread();
};

/*!
    This class is intended for use only with aio::AIOService
    \todo make it nested in aio::AIOService?
    \note All methods, except for \a pleaseStop(), must be called with \a mutex locked
*/
class NX_NETWORK_API AIOThread
:
    public AbstractAioThread,
    public QnLongRunnable
{
public:
    /*!
        \param mutex Mutex to use for exclusive access to internal data
    */
    AIOThread();
    virtual ~AIOThread();

    //!Implementation of QnLongRunnable::pleaseStop
    virtual void pleaseStop();
    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \note MUST be called with \a mutex locked
    */
    void watchSocket(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>() );
    //!Change timeout of existing polling \a sock for \a eventToWatch to \a timeoutMS. \a eventHandler is changed also
    /*!
        \note If \a sock is not polled, undefined behaviour can occur
    */
    void changeSocketTimeout(
        Pollable* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<Pollable>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0),
        std::function<void()> socketAddedToPollHandler = std::function<void()>() );
    //!Do not monitor \a sock for event \a eventType
    /*!
        Garantees that no \a eventTriggered will be called after return of this method.
        If \a eventTriggered is running and \a removeFromWatch called not from \a eventTriggered, method blocks till \a eventTriggered had returned
        \param waitForRunningHandlerCompletion See comment to \a aio::AIOService::removeFromWatch
        \note Calling this method with same parameters simultaneously from multiple threads can cause undefined behavour
        \note MUST be called with \a mutex locked
    */
    void removeFromWatch(
        Pollable* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler = nx::utils::MoveOnlyFunc<void()>());
    //!Queues \a functor to be executed from within this aio thread as soon as possible
    void post( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor );
    //!If called in this aio thread, then calls \a functor immediately, otherwise queues \a functor in same way as \a aio::AIOThread::post does
    void dispatch( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor );
    //!Cancels calls scheduled with \a aio::AIOThread::post and \a aio::AIOThread::dispatch
    void cancelPostedCalls( Pollable* const sock, bool waitForRunningHandlerCompletion );
    //!Returns number of sockets handled by this object
    size_t socketsHandled() const;

protected:
    //!Implementation of QThread::run
    virtual void run();

private:
    detail::AIOThreadImpl* m_impl;
};

}   //aio
}   //network
}   //nx

#endif  //AIOTHREAD_H
