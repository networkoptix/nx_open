/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"

#include <nx/utils/log/log.h>

#include "aio_thread_impl.h"


//TODO #ak memory order semantic used with std::atomic
//TODO #ak move task queues to socket for optimization
//TODO #ak should add some sequence which will be incremented when socket is started polling on any event
    //this is required to distinguish tasks. E.g., socket is polled, than polling stopped asynchronously, 
    //before async stop completion socket is polled again. In this case remove task should not remove tasks 
    //from new polling "session"? Something needs to be done about it


namespace nx {
namespace network {
namespace aio {

AbstractAioThread::~AbstractAioThread()
{
}



AIOThread::AIOThread()
:
    QnLongRunnable( false ),
    m_impl( new detail::AIOThreadImpl() )
{
    setObjectName(QString::fromLatin1("AIOThread") );
}

AIOThread::~AIOThread()
{
    pleaseStop();
    wait();

    delete m_impl;
    m_impl = NULL;
}

//!Implementation of QnLongRunnable::pleaseStop
void AIOThread::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_impl->pollSet.interrupt();
}

//!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
/*!
    \note MUST be called with \a mutex locked
*/
void AIOThread::watchSocket(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler<Pollable>* const eventHandler,
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_impl->mutex);

    //checking queue for reverse task for \a sock
    if (m_impl->removeReverseTask(sock, eventToWatch, detail::TaskType::tAdding, eventHandler, timeoutMs.count()))
        return;    //ignoring task

    m_impl->pollSetModificationQueue.push_back(typename detail::AIOThreadImpl::SocketAddRemoveTask(
        detail::TaskType::tAdding,
        sock,
        eventToWatch,
        eventHandler,
        timeoutMs.count(),
        nullptr,
        std::move(socketAddedToPollHandler)));
    if (eventToWatch == aio::etRead)
        ++m_impl->newReadMonitorTaskCount;
    else if (eventToWatch == aio::etWrite)
        ++m_impl->newWriteMonitorTaskCount;
    if (currentThreadSystemId() != systemThreadId())  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
        m_impl->pollSet.interrupt();
}

//!Change timeout of existing polling \a sock for \a eventToWatch to \a timeoutMS. \a eventHandler is changed also
/*!
    \note If \a sock is not polled, undefined behaviour can occur
*/
void AIOThread::changeSocketTimeout(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler<Pollable>* const eventHandler,
    std::chrono::milliseconds timeoutMs,
    std::function<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_impl->mutex);
    //TODO #ak looks like copy-paste of previous method. Remove copy-paste nahuy!!!

    //removing other timeouts from queue
    //m_impl->removeReverseTask(
    //    sock, aio::etTimedOut, detail::TaskType::tRemoving, nullptr, 0);

    //if socket is marked for removal, not adding task 
    //TODO #ak is it needed?
    //void* userData = sock->impl()->eventTypeToUserData[eventToWatch];
    //NX_ASSERT(userData != nullptr);  //socket is not polled, but someone wants to change timeout
    //if (static_cast<AIOEventHandlingDataHolder*>(userData)->data->markedForRemoval.load(std::memory_order_relaxed) > 0)
    //    return;   //socket marked for removal, ignoring timeout change (like, cancelling it right now)

    m_impl->pollSetModificationQueue.push_back(typename detail::AIOThreadImpl::SocketAddRemoveTask(
        detail::TaskType::tChangingTimeout,
        sock,
        eventToWatch,
        eventHandler,
        timeoutMs.count(),
        nullptr,
        std::move(socketAddedToPollHandler)));
    if (currentThreadSystemId() != systemThreadId())  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
        m_impl->pollSet.interrupt();
}

//!Do not monitor \a sock for event \a eventType
/*!
    Garantees that no \a eventTriggered will be called after return of this method.
    If \a eventTriggered is running and \a removeFromWatch called not from \a eventTriggered, method blocks till \a eventTriggered had returned
    \param waitForRunningHandlerCompletion See comment to \a aio::AIOService::removeFromWatch
    \note Calling this method with same parameters simultaneously from multiple threads can cause undefined behavour
    \note MUST be called with \a mutex locked
*/
void AIOThread::removeFromWatch(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    QnMutexLocker lk(&m_impl->mutex);

    //checking queue for reverse task for \a sock
    if (m_impl->removeReverseTask(sock, eventType, detail::TaskType::tRemoving, NULL, 0))
        return;    //ignoring task

    void*& userData = sock->impl()->eventTypeToUserData[eventType];
    NX_ASSERT(userData != NULL);   //socket is not polled. NX_ASSERT?
    std::shared_ptr<detail::AIOEventHandlingData> handlingData = static_cast<detail::AIOEventHandlingDataHolder*>(userData)->data;
    if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0)
        return;   //socket already marked for removal
    ++handlingData->markedForRemoval;

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();

    //inAIOThread is false in case async operation cancellation. In most cases, inAIOThread is true
    if (inAIOThread)
    {
        lk.unlock();
        //removing socket from pollset does not invalidate iterators (iterating pollset may be higher the stack)
        m_impl->removeSocketFromPollSet(sock, eventType);
        userData = nullptr;
        return;
    }
        
    if (waitForRunningHandlerCompletion)
    {
        std::atomic<int> taskCompletedCondition(0);
        //we MUST remove socket from pollset before returning from here
        m_impl->pollSetModificationQueue.push_back(typename detail::AIOThreadImpl::SocketAddRemoveTask(
            detail::TaskType::tRemoving,
            sock,
            eventType,
            nullptr,
            0,
            &taskCompletedCondition));

        m_impl->pollSet.interrupt();

        //we can be sure that socket will be removed before next poll

        lk.unlock();

        //TODO #ak maybe we just have to wait for remove completion, but not for running handler completion?
        //I.e., is it possible that handler was launched (or still running) after removal task completion?

        //waiting for event handler completion (if it running)
        while (handlingData->beingProcessed.load() > 0)
        {
            m_impl->socketEventProcessingMutex.lock();
            m_impl->socketEventProcessingMutex.unlock();
        }

        //waiting for socket to be removed from pollset
        while (taskCompletedCondition.load(std::memory_order_relaxed) == 0)
            msleep(0);    //yield. TODO #ak Better replace it with conditional_variable
                            //TODO #ak if remove task completed, doesn't it mean handler is not running and never be launched?

        lk.relock();
    }
    else
    {
        m_impl->pollSetModificationQueue.push_back(typename detail::AIOThreadImpl::SocketAddRemoveTask(
            detail::TaskType::tRemoving,
            sock,
            eventType,
            nullptr,
            0,
            nullptr,
            std::move(pollingStoppedHandler)));
        m_impl->pollSet.interrupt();
    }
}

//!Queues \a functor to be executed from within this aio thread as soon as possible
void AIOThread::post( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor )
{
    QnMutexLocker lk(&m_impl->mutex);

    m_impl->pollSetModificationQueue.push_back(
        typename detail::AIOThreadImpl::PostAsyncCallTask(
            sock,
            std::move(functor)));
    //if eventTriggered is lower on stack, socket will be added to pollset before the next poll call
    if (currentThreadSystemId() != systemThreadId())
        m_impl->pollSet.interrupt();
}

//!If called in this aio thread, then calls \a functor immediately, otherwise queues \a functor in same way as \a aio::AIOThread::post does
void AIOThread::dispatch( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor )
{
    if (currentThreadSystemId() == systemThreadId())  //if called from this aio thread
    {
        functor();
        return;
    }
    //otherwise posting functor
    post(sock, std::move(functor));
}

//!Cancels calls scheduled with \a aio::AIOThread::post and \a aio::AIOThread::dispatch
void AIOThread::cancelPostedCalls( Pollable* const sock, bool waitForRunningHandlerCompletion )
{
    QnMutexLocker lk(&m_impl->mutex);

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();
    if (inAIOThread)
    {
        //removing postedCall tasks and posted calls
        auto postedCallsToRemove = m_impl->cancelPostedCallsInternal(
            &lk,
            sock->impl()->socketSequence);
        lk.unlock();
        //removing postedCallsToRemove with mutex unlocked since there can be indirect calls to this
        return;
    }
        
    if (waitForRunningHandlerCompletion)
    {
        //posting cancellation task
        m_impl->pollSetModificationQueue.push_back(
            typename detail::AIOThreadImpl::CancelPostedCallsTask(
                sock->impl()->socketSequence,   //not passing socket here since it is allowed to be removed
                                                //before posted call is actually cancelled
                nullptr));
        m_impl->pollSet.interrupt();

        //we can be sure that socket will be removed before next poll

        lk.unlock();

        //waiting for posted calls processing to finish
        while (m_impl->processingPostedCalls == 1)
            msleep(0);    //yield. TODO #ak Better replace it with conditional_variable
                            //TODO #ak must wait for target call only, not for any call!

                            //here we can be sure that posted call for socket will never be triggered.
                            //  Although, it may still be in the queue.
                            //  But, socket can be safely removed, since we use socketSequence

        lk.relock();
    }
    else
    {
        m_impl->pollSetModificationQueue.push_back(
            typename detail::AIOThreadImpl::CancelPostedCallsTask(sock->impl()->socketSequence));
        m_impl->pollSet.interrupt();
    }
}

//!Returns number of sockets handled by this object
size_t AIOThread::socketsHandled() const
{
    return m_impl->pollSet.size() + m_impl->newReadMonitorTaskCount + m_impl->newWriteMonitorTaskCount;
}

void AIOThread::run()
{
    static const int ERROR_RESET_TIMEOUT = 1000;

    initSystemThreadId();
    NX_LOG(QLatin1String("AIO thread started"), cl_logDEBUG1);

    while (!needToStop())
    {
        //setting processingPostedCalls flag before processPollSetModificationQueue 
        //  to be able to atomically add "cancel posted call" task and check for tasks to complete
        m_impl->processingPostedCalls = 1;

        m_impl->processPollSetModificationQueue(detail::TaskType::tAll);

        //making calls posted with post and dispatch
        m_impl->processPostedCalls();

        m_impl->processingPostedCalls = 0;

        //processing tasks that have been added from within \a processPostedCalls() call
        m_impl->processPollSetModificationQueue(detail::TaskType::tAll);

        qint64 curClock = m_impl->getSystemTimerVal();
        //taking clock of the next periodic task
        qint64 nextPeriodicEventClock = 0;
        {
            QnMutexLocker lk(&m_impl->socketEventProcessingMutex);
            nextPeriodicEventClock = m_impl->periodicTasksByClock.empty() ? 0 : m_impl->periodicTasksByClock.cbegin()->first;
        }

        //calculating delay to the next periodic task
        const int millisToTheNextPeriodicEvent = nextPeriodicEventClock == 0
            ? aio::INFINITE_TIMEOUT    //no periodic task
            : (nextPeriodicEventClock < curClock ? 0 : nextPeriodicEventClock - curClock);

        //if there are posted calls, just checking sockets state in non-blocking mode
        const int pollTimeout = m_impl->postedCalls.empty() ? millisToTheNextPeriodicEvent : 0;
        const int triggeredSocketCount = m_impl->pollSet.poll(pollTimeout);

        if (needToStop())
            break;
        if (triggeredSocketCount < 0)
        {
            const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
            if (errorCode == SystemError::interrupted)
                continue;
            NX_LOG(QString::fromLatin1("AIOThread. poll failed. %1").arg(SystemError::toString(errorCode)), cl_logDEBUG1);
            msleep(ERROR_RESET_TIMEOUT);
            continue;
        }
        curClock = m_impl->getSystemTimerVal();
        if (triggeredSocketCount == 0)
        {
            if (nextPeriodicEventClock == 0 ||      //no periodic event
                nextPeriodicEventClock > curClock) //not a time for periodic event
            {
                continue;   //poll has been interrupted
            }
        }

        m_impl->removeSocketsFromPollSet();

        if (m_impl->processPeriodicTasks(curClock))
            continue;   //periodic task handler is allowed to delete socket what can cause undefined behavour while iterating pollset
        if (triggeredSocketCount > 0)
            m_impl->processSocketEvents(curClock);
    }

    NX_LOG(QLatin1String("AIO thread stopped"), cl_logDEBUG1);
}

}   //aio
}   //network
}   //nx
