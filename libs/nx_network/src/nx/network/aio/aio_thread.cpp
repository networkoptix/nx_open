#include "aio_thread.h"

#include <chrono>
#include <thread>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "aio_task_queue.h"
#include "pollset_factory.h"

// TODO: #ak Memory order semantic used with std::atomic.
// TODO: #ak Move task queues to socket for optimization.

namespace nx::network::aio {

AioThread::AioThread(std::unique_ptr<AbstractPollSet> pollSet):
    m_pollSet(pollSet ? std::move(pollSet) : PollSetFactory::instance()->create()),
    m_taskQueue(std::make_unique<detail::AioTaskQueue>(m_pollSet.get()))
{
    setObjectName(QString::fromLatin1("AioThread"));
}

AioThread::~AioThread()
{
    pleaseStop();
    wait();

    NX_ASSERT(m_taskQueue->empty());
}

void AioThread::pleaseStop()
{
    nx::utils::Thread::pleaseStop();
    m_pollSet->interrupt();
}

void AioThread::startMonitoring(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::optional<std::chrono::milliseconds> timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    if (!timeoutMillis)
    {
        timeoutMillis = std::chrono::milliseconds::zero();
        if (!getSocketTimeout(sock, eventToWatch, &(*timeoutMillis)))
        {
            post(
                sock,
                std::bind(&AIOEventHandler::eventTriggered, eventHandler, sock, aio::etError));
            return;
        }
    }

    QnMutexLocker lock(&m_taskQueue->mutex);

    if (sock->impl()->monitoredEvents[eventToWatch].isUsed)
    {
        if (sock->impl()->monitoredEvents[eventToWatch].timeout == timeoutMillis)
            return;
        sock->impl()->monitoredEvents[eventToWatch].timeout = timeoutMillis;
        changeSocketTimeout(
            lock,
            sock,
            eventToWatch,
            eventHandler,
            *timeoutMillis);
    }
    else
    {
        sock->impl()->monitoredEvents[eventToWatch].isUsed = true;
        startMonitoringInternal(
            lock,
            sock,
            eventToWatch,
            eventHandler,
            *timeoutMillis,
            std::move(socketAddedToPollHandler));
    }
}

void AioThread::stopMonitoring(
    Pollable* const sock,
    aio::EventType eventType)
{
    QnMutexLocker lock(&m_taskQueue->mutex);

    if (sock->impl()->monitoredEvents[eventType].isUsed)
    {
        sock->impl()->monitoredEvents[eventType].isUsed = false;
        stopMonitoringInternal(
            &lock,
            sock,
            eventType);
    }
}

void AioThread::post(Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor)
{
    QnMutexLocker lock(&m_taskQueue->mutex);
    post(lock, sock, std::move(functor));
}

void AioThread::dispatch(Pollable* const sock, nx::utils::MoveOnlyFunc<void()> functor)
{
    if (currentThreadSystemId() == systemThreadId())  //< If called from this aio thread.
    {
        functor();
        return;
    }
    // Otherwise posting functor.
    post(sock, std::move(functor));
}

void AioThread::cancelPostedCalls(Pollable* const sock)
{
    QnMutexLocker lock(&m_taskQueue->mutex);

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();
    if (inAIOThread)
    {
        // Removing postedCall tasks and posted calls.
        auto postedCallsToRemove = m_taskQueue->cancelPostedCalls(
            lock,
            sock->impl()->socketSequence);
        lock.unlock();
        // Removing postedCallsToRemove with mutex unlocked since there can be indirect calls to this.
        return;
    }

    // Posting cancellation task
    m_taskQueue->addTask(
        detail::CancelPostedCallsTask(
            sock->impl()->socketSequence, //< Not passing socket here since it is allowed to be removed
                                          // before posted call is actually cancelled.
            nullptr));
    m_pollSet->interrupt();

    // We can be sure that socket will be removed before next poll.

    lock.unlock();

    // Waiting for posted calls processing to finish.
    while (m_processingPostedCalls == 1)
        msleep(0);    // yield. TODO: #ak Better replace it with conditional_variable.
                      // TODO: #ak Must wait for target call only, not for any call!

                      // Here we can be sure that posted call for socket will never be triggered.
                      // Although, it may still be in the queue.
                      // But, socket can be safely removed, since we use socketSequence.

    lock.relock();
}

size_t AioThread::socketsHandled() const
{
    QnMutexLocker lock(&m_taskQueue->mutex);
    return m_pollSet->size()
        + m_taskQueue->newReadMonitorTaskCount
        + m_taskQueue->newWriteMonitorTaskCount;
}

bool AioThread::isSocketBeingMonitored(Pollable* sock) const
{
    for (const auto& monitoringContext: sock->impl()->monitoredEvents)
    {
        if (monitoringContext.isUsed)
            return true;
    }

    return false;
}

const detail::AioTaskQueue& AioThread::taskQueue() const
{
    return *m_taskQueue;
}

static constexpr std::chrono::milliseconds kErrorResetTimeout(1);

void AioThread::run()
{
    initSystemThreadId();
    NX_DEBUG(this, "AIO thread started");

    while (!needToStop())
    {
        //setting m_processingPostedCalls flag before processPollSetModificationQueue
        //  to be able to atomically add "cancel posted call" task and check for tasks to complete
        m_processingPostedCalls = 1;

        m_taskQueue->processPollSetModificationQueue(detail::TaskType::tAll);

        //making calls posted with post and dispatch
        m_taskQueue->processPostedCalls();

        m_processingPostedCalls = 0;

        //processing tasks that have been added from within processPostedCalls() call
        m_taskQueue->processPollSetModificationQueue(detail::TaskType::tAll);

        qint64 curClock = m_taskQueue->getMonotonicTime();
        const auto nextPeriodicEventClock = m_taskQueue->nextPeriodicEventClock();

        //calculating delay to the next periodic task
        const int millisToTheNextPeriodicEvent = nextPeriodicEventClock == 0
            ? aio::kInfiniteTimeout    //no periodic task
            : (nextPeriodicEventClock < curClock ? 0 : nextPeriodicEventClock - curClock);

        //if there are posted calls, just checking sockets state in non-blocking mode
        const int pollTimeout = (m_taskQueue->postedCallCount() == 0) ? millisToTheNextPeriodicEvent : 0;
        const int triggeredSocketCount = m_pollSet->poll(pollTimeout);

        if (needToStop())
            break;
        if (triggeredSocketCount < 0)
        {
            const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
            if (errorCode == SystemError::interrupted)
                continue;
            NX_DEBUG(this, lm("poll failed. %1").args(SystemError::toString(errorCode)));
            std::this_thread::sleep_for(kErrorResetTimeout);
            continue;
        }
        curClock = m_taskQueue->getMonotonicTime();
        if (triggeredSocketCount == 0)
        {
            if (nextPeriodicEventClock == 0 ||      //no periodic event
                nextPeriodicEventClock > curClock)  //not a time for periodic event
            {
                continue;   //poll has been interrupted
            }
        }

        m_taskQueue->processScheduledRemoveSocketTasks();

        if (triggeredSocketCount > 0)
            m_taskQueue->processSocketEvents(curClock);

        if (m_taskQueue->processPeriodicTasks(curClock))
            continue;
    }

    // Making sure every completion handler is removed within AIO thread.
    // Since it can own some socket.
    m_taskQueue->clear();

    NX_DEBUG(this, "AIO thread stopped");
}

bool AioThread::getSocketTimeout(
    Pollable* const sock,
    aio::EventType eventToWatch,
    std::chrono::milliseconds* timeout)
{
    unsigned int sockTimeoutMS = 0;
    if (eventToWatch == aio::etRead)
    {
        if (!sock->getRecvTimeout(&sockTimeoutMS))
            return false;
    }
    else if (eventToWatch == aio::etWrite)
    {
        if (!sock->getSendTimeout(&sockTimeoutMS))
            return false;
    }
    else
    {
        NX_ASSERT(false);
        return false;
    }
    *timeout = std::chrono::milliseconds(sockTimeoutMS);
    return true;
}

void AioThread::startMonitoringInternal(
    const QnMutexLockerBase& /*lock*/,
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    // TODO: #ak Make following check complexity constant-time.
    if (m_taskQueue->taskExists(sock, eventToWatch, detail::TaskType::tAdding))
        return;

    // Checking queue for reverse task for sock.
    if (m_taskQueue->removeReverseTask(sock, eventToWatch, detail::TaskType::tAdding, eventHandler, timeout))
        return;    //< Ignoring task.

    m_taskQueue->addTask(detail::SocketAddRemoveTask(
        detail::TaskType::tAdding,
        sock,
        eventToWatch,
        eventHandler,
        timeout,
        nullptr,
        std::move(socketAddedToPollHandler)));
    if (eventToWatch == aio::etRead)
        ++m_taskQueue->newReadMonitorTaskCount;
    else if (eventToWatch == aio::etWrite)
        ++m_taskQueue->newWriteMonitorTaskCount;
    if (currentThreadSystemId() != systemThreadId())  //< If eventTriggered is lower on stack, socket will be added to pollset before next poll call.
        m_pollSet->interrupt();
}

void AioThread::stopMonitoringInternal(
    QnMutexLockerBase* lock,
    Pollable* const sock,
    aio::EventType eventType)
{
    // Checking queue for reverse task for sock.
    if (m_taskQueue->removeReverseTask(
            sock, eventType, detail::TaskType::tRemoving,
            nullptr, std::chrono::milliseconds::zero()))
    {
        return;    //< Ignoring task.
    }

    auto handlingData = sock->impl()->monitoredEvents[eventType].aioHelperData.get();
    if (!handlingData)
        return; //< Socket is not polled.

    if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0)
        return;   // Socket already marked for removal.
    ++handlingData->markedForRemoval;

    const bool inAIOThread = currentThreadSystemId() == systemThreadId();

    // inAIOThread is false in case async operation cancellation. In most cases, inAIOThread is true.
    if (inAIOThread)
    {
        lock->unlock();
        // Removing socket from pollset does not invalidate iterators (iterating pollset may be higher the stack).
        m_taskQueue->removeSocketFromPollSet(sock, eventType);
        return;
    }

    std::atomic<int> taskCompletedCondition(0);
    // We MUST remove socket from pollset before returning from here.
    m_taskQueue->addTask(detail::SocketAddRemoveTask(
        detail::TaskType::tRemoving,
        sock,
        eventType,
        nullptr,
        std::chrono::milliseconds::zero(),
        &taskCompletedCondition));

    m_pollSet->interrupt();

    // We can be sure that socket will be removed before next poll.

    lock->unlock();

    // Waiting for socket to be removed from pollset.
    while (taskCompletedCondition.load(std::memory_order_relaxed) == 0)
        msleep(0); // Yield. TODO #ak Probably, it is better to replace it with conditional_variable.

    lock->relock();
}

void AioThread::changeSocketTimeout(
    const QnMutexLockerBase& /*lock*/,
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::chrono::milliseconds timeout,
    std::function<void()> socketAddedToPollHandler)
{
    m_taskQueue->addTask(detail::SocketAddRemoveTask(
        detail::TaskType::tChangingTimeout,
        sock,
        eventToWatch,
        eventHandler,
        timeout,
        nullptr,
        std::move(socketAddedToPollHandler)));
    // If eventTriggered call is down the stack, socket will be added to pollset before next poll call.
    if (currentThreadSystemId() != systemThreadId())
        m_pollSet->interrupt();
}

void AioThread::post(
    const QnMutexLockerBase& /*lock*/,
    Pollable* const sock,
    nx::utils::MoveOnlyFunc<void()> functor)
{
    m_taskQueue->addTask(
        detail::PostAsyncCallTask(sock, std::move(functor)));

    // If eventTriggered is lower on stack, socket will be added to pollset before the next poll call.
    if (currentThreadSystemId() != systemThreadId())
        m_pollSet->interrupt();
}

} // namespace nx::network::aio
