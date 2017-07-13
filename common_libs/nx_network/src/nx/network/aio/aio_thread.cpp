#include "aio_thread.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "aio_task_queue.h"

// TODO #ak Memory order semantic used with std::atomic.
// TODO #ak Move task queues to socket for optimization.

namespace nx {
namespace network {
namespace aio {

AIOThread::AIOThread(std::unique_ptr<AbstractPollSet> pollSet):
    QnLongRunnable(false),
    m_taskQueue(std::make_unique<detail::AioTaskQueue>(std::move(pollSet)))
{
    setObjectName(QString::fromLatin1("AIOThread"));
}

AIOThread::~AIOThread()
{
    pleaseStop();
    wait();
}

void AIOThread::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_taskQueue->pollSet->interrupt();
}

void AIOThread::watchSocket(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_taskQueue->mutex);

    // Checking queue for reverse task for sock.
    if (m_taskQueue->removeReverseTask(sock, eventToWatch, detail::TaskType::tAdding, eventHandler, timeoutMs.count()))
        return;    //< Ignoring task.

    m_taskQueue->addTask(detail::SocketAddRemoveTask(
        detail::TaskType::tAdding,
        sock,
        eventToWatch,
        eventHandler,
        timeoutMs.count(),
        nullptr,
        std::move(socketAddedToPollHandler)));
    if (eventToWatch == aio::etRead)
        ++m_taskQueue->newReadMonitorTaskCount;
    else if (eventToWatch == aio::etWrite)
        ++m_taskQueue->newWriteMonitorTaskCount;
    if (currentThreadSystemId() != systemThreadId())  //< If eventTriggered is lower on stack, socket will be added to pollset before next poll call.
        m_taskQueue->pollSet->interrupt();
}

void AIOThread::changeSocketTimeout(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::chrono::milliseconds timeoutMs,
    std::function<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_taskQueue->mutex);

    m_taskQueue->addTask(detail::SocketAddRemoveTask(
        detail::TaskType::tChangingTimeout,
        sock,
        eventToWatch,
        eventHandler,
        timeoutMs.count(),
        nullptr,
        std::move(socketAddedToPollHandler)));
    // If eventTriggered call is down the stack, socket will be added to pollset before next poll call.
    if (currentThreadSystemId() != systemThreadId())
        m_taskQueue->pollSet->interrupt();
}

void AIOThread::removeFromWatch(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    QnMutexLocker lk(&m_taskQueue->mutex);

    // Checking queue for reverse task for sock.
    if (m_taskQueue->removeReverseTask(sock, eventType, detail::TaskType::tRemoving, NULL, 0))
        return;    //< Ignoring task.

    void*& userData = sock->impl()->eventTypeToUserData[eventType];
    NX_ASSERT(userData != NULL);   //< Socket is not polled. NX_ASSERT?
    std::shared_ptr<detail::AioEventHandlingData> handlingData = static_cast<detail::AioEventHandlingDataHolder*>(userData)->data;
    if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0)
        return;   // Socket already marked for removal.
    ++handlingData->markedForRemoval;

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();

    // inAIOThread is false in case async operation cancellation. In most cases, inAIOThread is true.
    if (inAIOThread)
    {
        lk.unlock();
        // Removing socket from pollset does not invalidate iterators (iterating pollset may be higher the stack).
        m_taskQueue->removeSocketFromPollSet(sock, eventType);
        userData = nullptr;
        return;
    }
        
    if (waitForRunningHandlerCompletion)
    {
        std::atomic<int> taskCompletedCondition(0);
        // We MUST remove socket from pollset before returning from here.
        m_taskQueue->addTask(detail::SocketAddRemoveTask(
            detail::TaskType::tRemoving,
            sock,
            eventType,
            nullptr,
            0,
            &taskCompletedCondition));

        m_taskQueue->pollSet->interrupt();

        // We can be sure that socket will be removed before next poll.

        lk.unlock();

        // TODO #ak maybe we just have to wait for remove completion, but not for running handler completion?
        // I.e., is it possible that handler was launched (or still running) after removal task completion?

        // Waiting for event handler completion (if it running).
        while (handlingData->beingProcessed.load() > 0)
        {
            m_taskQueue->socketEventProcessingMutex.lock();
            m_taskQueue->socketEventProcessingMutex.unlock();
        }

        // Waiting for socket to be removed from pollset.
        while (taskCompletedCondition.load(std::memory_order_relaxed) == 0)
            msleep(0);    // Yield. TODO #ak Better replace it with conditional_variable.
                          // TODO #ak if remove task completed, doesn't it mean handler is not running and never be launched?

        lk.relock();
    }
    else
    {
        m_taskQueue->addTask(detail::SocketAddRemoveTask(
            detail::TaskType::tRemoving,
            sock,
            eventType,
            nullptr,
            0,
            nullptr,
            std::move(pollingStoppedHandler)));
        m_taskQueue->pollSet->interrupt();
    }
}

void AIOThread::post( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor )
{
    QnMutexLocker lk(&m_taskQueue->mutex);

    m_taskQueue->addTask(
        detail::PostAsyncCallTask(
            sock,
            std::move(functor)));
    // If eventTriggered is lower on stack, socket will be added to pollset before the next poll call.
    if (currentThreadSystemId() != systemThreadId())
        m_taskQueue->pollSet->interrupt();
}

void AIOThread::dispatch( Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor )
{
    if (currentThreadSystemId() == systemThreadId())  //< If called from this aio thread.
    {
        functor();
        return;
    }
    // Otherwise posting functor.
    post(sock, std::move(functor));
}

void AIOThread::cancelPostedCalls( Pollable* const sock, bool waitForRunningHandlerCompletion )
{
    QnMutexLocker lk(&m_taskQueue->mutex);

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();
    if (inAIOThread)
    {
        // Removing postedCall tasks and posted calls
        auto postedCallsToRemove = m_taskQueue->cancelPostedCalls(
            lk,
            sock->impl()->socketSequence);
        lk.unlock();
        // Removing postedCallsToRemove with mutex unlocked since there can be indirect calls to this.
        return;
    }
        
    if (waitForRunningHandlerCompletion)
    {
        // Posting cancellation task
        m_taskQueue->addTask(
            detail::CancelPostedCallsTask(
                sock->impl()->socketSequence,   //not passing socket here since it is allowed to be removed
                                                //before posted call is actually cancelled
                nullptr));
        m_taskQueue->pollSet->interrupt();

        //we can be sure that socket will be removed before next poll

        lk.unlock();

        //waiting for posted calls processing to finish
        while (m_taskQueue->processingPostedCalls == 1)
            msleep(0);    //yield. TODO #ak Better replace it with conditional_variable
                            //TODO #ak must wait for target call only, not for any call!

                            //here we can be sure that posted call for socket will never be triggered.
                            //  Although, it may still be in the queue.
                            //  But, socket can be safely removed, since we use socketSequence

        lk.relock();
    }
    else
    {
        m_taskQueue->addTask(
            detail::CancelPostedCallsTask(
                sock->impl()->socketSequence));
        m_taskQueue->pollSet->interrupt();
    }
}

size_t AIOThread::socketsHandled() const
{
    return m_taskQueue->pollSet->size() + m_taskQueue->newReadMonitorTaskCount + m_taskQueue->newWriteMonitorTaskCount;
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
        m_taskQueue->processingPostedCalls = 1;

        m_taskQueue->processPollSetModificationQueue(detail::TaskType::tAll);

        //making calls posted with post and dispatch
        m_taskQueue->processPostedCalls();

        m_taskQueue->processingPostedCalls = 0;

        //processing tasks that have been added from within \a processPostedCalls() call
        m_taskQueue->processPollSetModificationQueue(detail::TaskType::tAll);

        qint64 curClock = m_taskQueue->getSystemTimerVal();
        //taking clock of the next periodic task
        qint64 nextPeriodicEventClock = 0;
        {
            QnMutexLocker lk(&m_taskQueue->socketEventProcessingMutex);
            nextPeriodicEventClock = m_taskQueue->periodicTasksByClock.empty() ? 0 : m_taskQueue->periodicTasksByClock.cbegin()->first;
        }

        //calculating delay to the next periodic task
        const int millisToTheNextPeriodicEvent = nextPeriodicEventClock == 0
            ? aio::kInfiniteTimeout    //no periodic task
            : (nextPeriodicEventClock < curClock ? 0 : nextPeriodicEventClock - curClock);

        //if there are posted calls, just checking sockets state in non-blocking mode
        const int pollTimeout = (m_taskQueue->postedCallCount() == 0) ? millisToTheNextPeriodicEvent : 0;
        const int triggeredSocketCount = m_taskQueue->pollSet->poll(pollTimeout);

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
        curClock = m_taskQueue->getSystemTimerVal();
        if (triggeredSocketCount == 0)
        {
            if (nextPeriodicEventClock == 0 ||      //no periodic event
                nextPeriodicEventClock > curClock) //not a time for periodic event
            {
                continue;   //poll has been interrupted
            }
        }

        m_taskQueue->processScheduledRemoveSocketTasks();

        if (m_taskQueue->processPeriodicTasks(curClock))
            continue;   //periodic task handler is allowed to delete socket what can cause undefined behavour while iterating pollset
        if (triggeredSocketCount > 0)
            m_taskQueue->processSocketEvents(curClock);
    }

    NX_LOG(QLatin1String("AIO thread stopped"), cl_logDEBUG1);
}

} // namespace aio
} // namespace network
} // namespace nx
