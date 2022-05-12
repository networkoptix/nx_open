// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aio_task_queue.h"

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/time.h>

namespace nx::network::aio::detail {

static constexpr int kAbnormalProcessTimeFactor = 1000;
static constexpr auto kAbnormalProcessTimeDetectionPeriod = std::chrono::seconds(20);

AioTaskQueue::AioTaskQueue(AbstractPollSet* pollSet):
    m_pollSet(pollSet),
    m_abnormalProcessingTimeDetector(
        kAbnormalProcessTimeFactor,
        kAbnormalProcessTimeDetectionPeriod,
        [this](auto&&... args)
        {
            reportAbnormalProcessingTime(std::forward<decltype(args)>(args)...);
        })
{
}

void AioTaskQueue::addTask(SocketAddRemoveTask task)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_pollSetModificationQueue.push_back(std::move(task));

    if (task.type == TaskType::tAdding)
    {
        if (task.eventType == aio::etRead)
            ++m_newReadMonitorTaskCount;
        else if (task.eventType == aio::etWrite)
            ++m_newWriteMonitorTaskCount;
    }
}

bool AioTaskQueue::taskExists(
    Pollable* const socket,
    aio::EventType eventType,
    TaskType taskType) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return std::any_of(
        m_pollSetModificationQueue.begin(), m_pollSetModificationQueue.end(),
        [socket, eventType, taskType](const auto& task)
        {
            return task.socket == socket && task.eventType == eventType && task.type == taskType;
        });
}

bool AioTaskQueue::removeReverseTask(
    Pollable* const sock,
    aio::EventType eventType,
    TaskType taskType,
    AIOEventHandler* const eventHandler,
    std::chrono::milliseconds newTimeout)
{
    // NOTE: Freeing tasks with no mutex lock since user handler destruction may produce
    // a recursive call to this object.
    std::deque<SocketAddRemoveTask> toRemove;

    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto it = m_pollSetModificationQueue.begin();
        it != m_pollSetModificationQueue.end();
        ++it)
    {
        if (!(it->socket == sock && it->eventType == eventType && taskType != it->type))
            continue;

        //removing reverse task (if any)
        if (taskType == TaskType::tAdding && it->type == TaskType::tRemoving)
        {
            //cancelling removing socket
            if (eventHandler != it->eventHandler)
                continue;   //event handler changed, cannot ignore task
                            //cancelling remove task

            NX_ASSERT(sock->impl()->monitoredEvents[eventType].aioHelperData);
            sock->impl()->monitoredEvents[eventType].aioHelperData->timeout = newTimeout;
            sock->impl()->monitoredEvents[eventType].aioHelperData->markedForRemoval.store(0);

            toRemove.push_back(std::move(*it));
            m_pollSetModificationQueue.erase(it);
            return true;
        }
        else if (taskType == TaskType::tRemoving &&
            (it->type == TaskType::tAdding || it->type == TaskType::tChangingTimeout))
        {
            //if( it->type == TaskType::tChangingTimeout ) cancelling scheduled tasks, since socket removal from poll is requested
            const TaskType foundTaskType = it->type;

            //cancelling adding socket
            toRemove.push_back(std::move(*it));
            it = m_pollSetModificationQueue.erase(it);

            //removing futher tChangingTimeout tasks
            for (;
                it != m_pollSetModificationQueue.end();
                )
            {
                if (it->socket == sock && it->eventType == eventType)
                {
                    NX_ASSERT(it->type == TaskType::tChangingTimeout);
                    toRemove.push_back(std::move(*it));
                    it = m_pollSetModificationQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            if (foundTaskType == TaskType::tAdding)
                return true;
            else
                return false;   //foundTaskEventType == TaskType::tChangingTimeout
        }
    }

    return false;
}

std::size_t AioTaskQueue::newReadMonitorTaskCount() const
{
    return m_newReadMonitorTaskCount;
}

std::size_t AioTaskQueue::newWriteMonitorTaskCount() const
{
    return m_newWriteMonitorTaskCount;
}

qint64 AioTaskQueue::nextPeriodicEventClock() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_periodicTasksByClock.empty()
        ? 0
        : m_periodicTasksByClock.cbegin()->first;
}

std::size_t AioTaskQueue::periodicTasksCount() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_periodicTasksByClock.size();
}

void AioTaskQueue::clear()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto postedCalls = std::exchange(m_postedCalls, {});
    auto pollSetModificationQueue = std::exchange(m_pollSetModificationQueue, {});
    auto periodicTasksByClock = std::exchange(m_periodicTasksByClock, {});

    lock.unlock();

    // Freeing everything without lock.
}

bool AioTaskQueue::empty() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_postedCalls.empty()
        && m_pollSetModificationQueue.empty()
        && m_periodicTasksByClock.empty();
}

void AioTaskQueue::processPollSetModificationQueue(TaskType taskFilter)
{
    // Removing these tasks after releasing mutex to avoid some recursive locks when
    // removing user handlers.
    std::vector<SocketAddRemoveTask> totalElementsToRemove;

    NX_MUTEX_LOCKER lock(&m_mutex);

    for (typename std::deque<SocketAddRemoveTask>::iterator
        it = m_pollSetModificationQueue.begin();
        it != m_pollSetModificationQueue.end();
        )
    {
        SocketAddRemoveTask& task = *it;
        if ((taskFilter != TaskType::tAll) && (task.type != taskFilter))
        {
            ++it;
            continue;
        }

        std::vector<SocketAddRemoveTask> elementsToRemove;

        switch (task.type)
        {
            case TaskType::tAdding:
                processAddTask(lock, task);
                break;

            case TaskType::tChangingTimeout:
                processChangeTimeoutTask(lock, task);
                break;

            case TaskType::tRemoving:
                processRemoveTask(lock, task);
                break;

            case TaskType::tCallFunc:
                processCallFuncTask(lock, task);
                break;

            case TaskType::tCancelPostedCalls:
                elementsToRemove = processCancelPostedCallTask(lock, task);
                break;

            default:
                NX_ASSERT(false);
        }

        if (!elementsToRemove.empty())
        {
            if (totalElementsToRemove.empty())
            {
                totalElementsToRemove = std::move(elementsToRemove);
            }
            else
            {
                totalElementsToRemove.reserve(totalElementsToRemove.size() + elementsToRemove.size());
                std::move(
                    elementsToRemove.begin(),
                    elementsToRemove.end(),
                    std::back_inserter(totalElementsToRemove));
            }
        }

        if (task.taskCompletionEvent)
            task.taskCompletionEvent->store(1, std::memory_order_relaxed);
        if (task.taskCompletionHandler)
            task.taskCompletionHandler();

        it = m_pollSetModificationQueue.erase(it);
    }
}

void AioTaskQueue::processScheduledRemoveSocketTasks()
{
    processPollSetModificationQueue(TaskType::tRemoving);
}

void AioTaskQueue::processSocketEvents(const qint64 curClock)
{
    auto it = m_pollSet->getSocketEventsIterator();
    while (it->next())
    {
        Pollable* const socket = it->socket();
        const aio::EventType sockEventType = it->eventReceived();
        const aio::EventType handlerToInvokeType =
            (sockEventType == aio::etRead || sockEventType == aio::etWrite)
            ? sockEventType
            : (socket->impl()->monitoredEvents[aio::etRead].aioHelperData //in case of error calling any handler
                ? aio::etRead
                : aio::etWrite);

        //TODO #akolesnikov in case of error pollSet reports etError once,
        //but two handlers may be registered for socket.
        //So, we must notify both (if they differ, probably).
        //Currently, leaving as-is since any socket installs single handler for both events

        //TODO #akolesnikov notify second handler (if any) and if it not removed by first handler

        // No need to lock mutex, since data is removed in this thread only.
        // Copying shared_ptr here because socket can be removed in eventTriggered call.
        auto handlingData = socket->impl()->monitoredEvents[handlerToInvokeType].aioHelperData;

        ++handlingData->beingProcessed;
        auto beingProcessedScopedGuard =
            nx::utils::makeScopeGuard([&handlingData]() { --handlingData->beingProcessed; });

        if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0) //socket has been removed from watch
            continue;

        //eventTriggered is allowed to call stopMonitoring which can remove socket from pollset
        callAndReportAbnormalProcessingTime(
            [&]() { handlingData->eventHandler->eventTriggered(socket, sockEventType); },
            "socket event");

        //updating socket's periodic task (it's garanteed that there is a periodic task for socket)
        if (handlingData->timeout > std::chrono::milliseconds::zero())
            handlingData->updatedPeriodicTaskClock = curClock + handlingData->timeout.count();

        // NOTE: element, this iterator points to, could be removed in eventTriggered call,
        // but it is still safe to increment this iterator
    }
}

bool AioTaskQueue::processPeriodicTasks(const qint64 curClock)
{
    int tasksProcessedCount = 0;

    for (;;)
    {
        auto periodicTaskData = takeNextExpiredPeriodicTask(curClock);
        if (!periodicTaskData)
            break;

        // TODO: #akolesnikov do we really need to copy shared_ptr here?
        auto handlingData = periodicTaskData->data;
        handlingData->nextTimeoutClock = 0;

        ++handlingData->beingProcessed;
        auto beingProcessedScopedGuard =
            nx::utils::makeScopeGuard([&handlingData]() { --handlingData->beingProcessed; });

        if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0) //task has been removed from watch
            continue;

        if (handlingData->updatedPeriodicTaskClock > 0)
        {
            //not processing task, but updating.
            if (handlingData->updatedPeriodicTaskClock > curClock)
            {
                //adding new task with updated clock
                NX_MUTEX_LOCKER lock(&m_mutex);
                addPeriodicTask(
                    lock,
                    handlingData->updatedPeriodicTaskClock,
                    handlingData,
                    periodicTaskData->socket,
                    periodicTaskData->eventType);
                handlingData->updatedPeriodicTaskClock = 0;
                continue;
            }

            handlingData->updatedPeriodicTaskClock = 0;
            //updated task has to be executed now
        }
        else if (handlingData->updatedPeriodicTaskClock == -1)
        {
            //cancelling periodic task
            handlingData->updatedPeriodicTaskClock = 0;
            continue;
        }

        if (periodicTaskData->socket)    //periodic event, associated with socket (e.g., socket operation timeout)
        {
            //NOTE socket is allowed to be removed in eventTriggered
            callAndReportAbnormalProcessingTime(
                [&]()
                {
                    handlingData->eventHandler->eventTriggered(
                        periodicTaskData->socket,
                        static_cast<aio::EventType>(periodicTaskData->eventType | aio::etTimedOut));
                },
                "timer");

            //adding periodic task with same timeout
            NX_MUTEX_LOCKER lock(&m_mutex);
            addPeriodicTask(
                lock,
                curClock + handlingData->timeout.count(),
                handlingData,
                periodicTaskData->socket,
                periodicTaskData->eventType);
            ++tasksProcessedCount;
        }
        //else
        //    periodicTaskData.periodicEventHandler->onTimeout( periodicTaskData.taskID );  //for periodic tasks not bound to socket
    }

    return tasksProcessedCount > 0;
}

void AioTaskQueue::processPostedCalls()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    while (!m_postedCalls.empty())
    {
        auto postHandler = std::move(m_postedCalls.begin()->postHandler);
        NX_ASSERT(!m_postedCalls.front().socket ||
            m_postedCalls.front().socket->isInSelfAioThread());
        m_postedCalls.erase(m_postedCalls.begin());

        // NOTE: User handler may cancel some calls, so m_postedCalls may change.
        // But, new calls cannot be added there (they are added via m_pollSetModificationQueue).

        nx::Unlocker<nx::Mutex> unlock(&lock);

        callAndReportAbnormalProcessingTime(
            [&]() { postHandler(); },
            "post");

        // Destroying user callback with no lock since it can produce recursive call to this object.
        postHandler = nullptr;
    }
}

void AioTaskQueue::removeSocketFromPollSet(
    Pollable* sock,
    aio::EventType eventType)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    removeSocketFromPollSet(lock, sock, eventType);
}

std::vector<SocketAddRemoveTask> AioTaskQueue::cancelPostedCalls(
    SocketSequence socketSequence)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return cancelPostedCalls(lock, socketSequence);
}

std::size_t AioTaskQueue::postedCallCount() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_postedCalls.size();
}

qint64 AioTaskQueue::getMonotonicTime()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        nx::utils::monotonicTime().time_since_epoch()).count();
}

//-------------------------------------------------------------------------------------------------
// private section.

void AioTaskQueue::processAddTask(
    const nx::Locker<nx::Mutex>& lock,
    SocketAddRemoveTask& task)
{
    if (task.eventType == aio::etRead)
        --m_newReadMonitorTaskCount;
    else if (task.eventType == aio::etWrite)
        --m_newWriteMonitorTaskCount;

    addSocketToPollset(
        lock,
        task.socket,
        task.eventType,
        task.timeout,
        task.eventHandler);
}

void AioTaskQueue::processChangeTimeoutTask(
    const nx::Locker<nx::Mutex>& lock,
    SocketAddRemoveTask& task)
{
    const auto& handlingData =
        task.socket->impl()->monitoredEvents[task.eventType].aioHelperData;
    // NOTE: We are in aio thread currently.
    if (task.timeout > std::chrono::milliseconds::zero())
    {
        // Adding/updating periodic task.
        if (handlingData->timeout > std::chrono::milliseconds::zero())
        {
            // Updating existing task.
            const auto newClock = getMonotonicTime() + task.timeout.count();
            if (handlingData->nextTimeoutClock == 0)
            {
                handlingData->updatedPeriodicTaskClock = newClock;
            }
            else
            {
                replacePeriodicTask(
                    lock,
                    handlingData,
                    newClock,
                    task.socket,
                    task.eventType);
            }
        }
        else
        {
            addPeriodicTask(
                lock,
                getMonotonicTime() + task.timeout.count(),
                handlingData,
                task.socket,
                task.eventType);
        }
    }
    else if (handlingData->timeout > std::chrono::milliseconds::zero())  //&& timeout == 0
    {
        // Cancelling existing periodic task (there must be one).
        handlingData->updatedPeriodicTaskClock = -1;
    }
    handlingData->timeout = task.timeout;
}

void AioTaskQueue::processRemoveTask(
    const nx::Locker<nx::Mutex>& lock,
    SocketAddRemoveTask& task)
{
    removeSocketFromPollSet(lock, task.socket, task.eventType);
}

void AioTaskQueue::processCallFuncTask(
    const nx::Locker<nx::Mutex>& /*lock*/,
    SocketAddRemoveTask& task)
{
    NX_ASSERT(task.postHandler);
    NX_ASSERT(!task.taskCompletionEvent && !task.taskCompletionHandler);
    m_postedCalls.push_back(std::move(task));

    // This task differs from every else in a way that it is not processed here,
    // just moved to another container.
    // TODO #akolesnikov Is it really needed to move to another container?
}

std::vector<SocketAddRemoveTask> AioTaskQueue::processCancelPostedCallTask(
    const nx::Locker<nx::Mutex>& lock,
    SocketAddRemoveTask& task)
{
    return cancelPostedCalls(lock, task.socketSequence);
}

//-------------------------------------------------------------------------------------------------

void AioTaskQueue::addSocketToPollset(
    const nx::Locker<nx::Mutex>& lock,
    Pollable* socket,
    aio::EventType eventType,
    std::chrono::milliseconds timeout,
    AIOEventHandler* eventHandler)
{
    auto handlingData = std::make_shared<AioEventHandlingData>(eventHandler);
    if (eventType != aio::etTimedOut)
    {
        if (!m_pollSet->add(socket, eventType))
        {
            const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
            NX_WARNING(this, nx::format("Failed to add %1 to pollset. %2")
                .args(socket, SystemError::toString(errorCode)));

            socket->impl()->monitoredEvents[eventType].isUsed = false;
            socket->impl()->monitoredEvents[eventType].timeout = std::nullopt;

            m_postedCalls.push_back(
                PostAsyncCallTask(
                    socket,
                    [eventHandler, socket]()
                    {
                        eventHandler->eventTriggered(socket, aio::etError);
                    }));
            return;
        }
    }

    if (timeout > std::chrono::milliseconds::zero())
    {
        // Adding periodic task associated with socket.
        handlingData->timeout = timeout;
        addPeriodicTask(
            lock,
            getMonotonicTime() + timeout.count(),
            handlingData,
            socket,
            eventType);
    }

    socket->impl()->monitoredEvents[eventType].aioHelperData = std::move(handlingData);
}

void AioTaskQueue::removeSocketFromPollSet(
    const nx::Locker<nx::Mutex>& lock,
    Pollable* sock,
    aio::EventType eventType)
{
    auto& handlingData = sock->impl()->monitoredEvents[eventType].aioHelperData;
    if (handlingData)
    {
        if (handlingData->nextTimeoutClock != 0)
            cancelPeriodicTask(lock, handlingData.get(), eventType);

        handlingData = nullptr;
    }

    if (eventType == aio::etRead || eventType == aio::etWrite)
        m_pollSet->remove(sock, eventType);
}

//-------------------------------------------------------------------------------------------------

void AioTaskQueue::addPeriodicTask(
    const nx::Locker<nx::Mutex>& /*lock*/,
    const qint64 taskClock,
    const std::shared_ptr<AioEventHandlingData>& handlingData,
    Pollable* _socket,
    aio::EventType eventType)
{
    handlingData->nextTimeoutClock = taskClock;
    m_periodicTasksByClock.emplace(
        taskClock,
        PeriodicTaskData(handlingData, _socket, eventType));
}

std::optional<PeriodicTaskData> AioTaskQueue::takeNextExpiredPeriodicTask(qint64 curClock)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // Taking task from queue.
    auto it = m_periodicTasksByClock.begin();
    if (it == m_periodicTasksByClock.end() || it->first > curClock)
        return std::nullopt;

    auto periodicTaskData = std::move(it->second);
    m_periodicTasksByClock.erase(it);

    return periodicTaskData;
}

void AioTaskQueue::replacePeriodicTask(
    const nx::Locker<nx::Mutex>& lock,
    const std::shared_ptr<AioEventHandlingData>& handlingData,
    qint64 newClock,
    Pollable* socket,
    aio::EventType eventType)
{
    for (auto it = m_periodicTasksByClock.lower_bound(handlingData->nextTimeoutClock);
        it != m_periodicTasksByClock.end() && it->first == handlingData->nextTimeoutClock;
        ++it)
    {
        if (it->second.socket == socket)
        {
            m_periodicTasksByClock.erase(it);
            addPeriodicTask(
                lock, newClock, handlingData, socket, eventType);
            break;
        }
    }
}

void AioTaskQueue::cancelPeriodicTask(
    const nx::Locker<nx::Mutex>& /*lock*/,
    AioEventHandlingData* handlingData,
    aio::EventType eventType)
{
    for (auto it = m_periodicTasksByClock.lower_bound(handlingData->nextTimeoutClock);
        it != m_periodicTasksByClock.end() && it->first == handlingData->nextTimeoutClock;
        ++it)
    {
        if (it->second.data.get() == handlingData && it->second.eventType == eventType)
        {
            m_periodicTasksByClock.erase(it);
            return;
        }
    }
}

//-------------------------------------------------------------------------------------------------

std::vector<SocketAddRemoveTask> AioTaskQueue::cancelPostedCalls(
    const nx::Locker<nx::Mutex>& /*lock*/,
    SocketSequence socketSequence)
{
    std::vector<SocketAddRemoveTask> elementsToRemove;

    // TODO: #akolesnikov This method has O(m_pollSetModificationQueue.size())
    // which can be quite large in Cloud, for example.
    // Should optimize it here.
    // Probably, it can become O(C) after some refactoring.

    //detecting range of elements to remove
    const auto tasksToRemoveRangeStart = nx::utils::move_if(
        m_pollSetModificationQueue.begin(),
        m_pollSetModificationQueue.end(),
        std::back_inserter(elementsToRemove),
        [socketSequence](const SocketAddRemoveTask& val)
        {
            return val.type == TaskType::tCallFunc
                && val.socketSequence == socketSequence;
        });
    m_pollSetModificationQueue.erase(
        tasksToRemoveRangeStart,
        m_pollSetModificationQueue.end());

    const auto postedCallsRemoveRangeStart = nx::utils::move_if(
        m_postedCalls.begin(),
        m_postedCalls.end(),
        std::back_inserter(elementsToRemove),
        [socketSequence](const SocketAddRemoveTask& val)
        {
            return val.socketSequence == socketSequence;
        });
    m_postedCalls.erase(
        postedCallsRemoveRangeStart,
        m_postedCalls.end());

    return elementsToRemove;
}

template<typename Func>
void AioTaskQueue::callAndReportAbnormalProcessingTime(
    Func func,
    const char* description)
{
    nx::utils::BasicElapsedTimer<std::chrono::microseconds> timer(
        nx::utils::ElapsedTimerState::started);
    func();
    m_abnormalProcessingTimeDetector.add(timer.elapsed(), description);
}

void AioTaskQueue::reportAbnormalProcessingTime(
    std::chrono::microseconds value,
    std::chrono::microseconds average,
    const char* where_)
{
    NX_DEBUG(this, "Abnormal %1 running time detected: %2 vs %3 on average",
        where_, value, average);
}

} // namespace nx::network::aio::detail
