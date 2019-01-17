#include "aio_task_queue.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::aio::detail {

AioTaskQueue::AioTaskQueue(AbstractPollSet* pollSet):
    m_pollSet(pollSet)
{
    m_monotonicClock.restart();
}

qint64 AioTaskQueue::getSystemTimerVal() const
{
    return m_monotonicClock.elapsed();
}

void AioTaskQueue::addTask(SocketAddRemoveTask task)
{
    m_pollSetModificationQueue.push_back(std::move(task));
}

bool AioTaskQueue::taskExists(
    Pollable* const socket,
    aio::EventType eventType,
    TaskType taskType) const
{
    for (const auto& task: m_pollSetModificationQueue)
    {
        if (task.socket == socket && task.eventType == eventType && task.type == taskType)
            return true;
    }
    return false;
}

void AioTaskQueue::postAsyncCall(
    Pollable* const pollable,
    nx::utils::MoveOnlyFunc<void()> func)
{
    addTask(PostAsyncCallTask(pollable, std::move(func)));
}

void AioTaskQueue::processPollSetModificationQueue(TaskType taskFilter)
{
    std::vector<SocketAddRemoveTask> elementsToRemove;
    QnMutexLocker lk(&mutex);

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

        switch (task.type)
        {
            case TaskType::tAdding:
            {
                if (task.eventType == aio::etRead)
                    --newReadMonitorTaskCount;
                else if (task.eventType == aio::etWrite)
                    --newWriteMonitorTaskCount;
                addSocketToPollset(
                    task.socket,
                    task.eventType,
                    task.timeout,
                    task.eventHandler);
                break;
            }

            case TaskType::tChangingTimeout:
            {
                void* userData = task.socket->impl()->monitoredEvents[task.eventType].userData;
                AioEventHandlingDataHolder* handlingData =
                    reinterpret_cast<AioEventHandlingDataHolder*>(userData);
                // NOTE: We are in aio thread currently.
                if (task.timeout > 0)
                {
                    // Adding/updating periodic task.
                    if (handlingData->data->timeout > 0)
                    {
                        // Updating existing task.
                        const auto newClock = getSystemTimerVal() + task.timeout;
                        if (handlingData->data->nextTimeoutClock == 0)
                        {
                            handlingData->data->updatedPeriodicTaskClock = newClock;
                        }
                        else
                        {
                            // Replacing timer record in m_periodicTasksByClock.
                            for (auto it = m_periodicTasksByClock.lower_bound(handlingData->data->nextTimeoutClock);
                                it != m_periodicTasksByClock.end() && it->first == handlingData->data->nextTimeoutClock;
                                ++it)
                            {
                                if (it->second.socket == task.socket)
                                {
                                    m_periodicTasksByClock.erase(it);
                                    addPeriodicTaskNonSafe(
                                        newClock, handlingData->data, task.socket, task.eventType);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        addPeriodicTask(
                            getSystemTimerVal() + task.timeout,
                            handlingData->data,
                            task.socket,
                            task.eventType);
                    }
                }
                else if (handlingData->data->timeout > 0)  //&& timeout == 0
                {
                    // Cancelling existing periodic task (there must be one).
                    handlingData->data->updatedPeriodicTaskClock = -1;
                }
                handlingData->data->timeout = task.timeout;
                break;
            }

            case TaskType::tRemoving:
                removeSocketFromPollSet(task.socket, task.eventType);
                break;

            case TaskType::tCallFunc:
            {
                NX_ASSERT(task.postHandler);
                NX_ASSERT(!task.taskCompletionEvent && !task.taskCompletionHandler);
                m_postedCalls.push_back(std::move(task));
                // This task differs from every else in a way that it is not processed here,
                // just moved to another container. 
                // TODO #ak Is it really needed to move to another container?
                it = m_pollSetModificationQueue.erase(it);
                continue;
            }

            case TaskType::tCancelPostedCalls:
            {
                auto postedCallsToRemove = cancelPostedCalls(lk, task.socketSequence);
                if (elementsToRemove.empty())
                {
                    elementsToRemove = std::move(postedCallsToRemove);
                }
                else
                {
                    elementsToRemove.reserve(elementsToRemove.size() + postedCallsToRemove.size());
                    std::move(
                        postedCallsToRemove.begin(),
                        postedCallsToRemove.end(),
                        std::back_inserter(elementsToRemove));
                }
                break;
            }

            default:
                NX_ASSERT(false);
        }
        if (task.taskCompletionEvent)
            task.taskCompletionEvent->store(1, std::memory_order_relaxed);
        if (task.taskCompletionHandler)
            task.taskCompletionHandler();
        it = m_pollSetModificationQueue.erase(it);
    }
}

void AioTaskQueue::addSocketToPollset(
    Pollable* socket,
    aio::EventType eventType,
    int timeout,
    AIOEventHandler* eventHandler)
{
    auto handlingData = std::make_unique<AioEventHandlingDataHolder>(eventHandler);
    if (eventType != aio::etTimedOut)
    {
        if (!m_pollSet->add(socket, eventType, handlingData.get()))
        {
            const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
            NX_WARNING(this, lm("Failed to add %1 to pollset. %2")
                .args(socket, SystemError::toString(errorCode)));

            socket->impl()->monitoredEvents[eventType].isUsed = false;
            socket->impl()->monitoredEvents[eventType].timeout = boost::none;

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

    socket->impl()->monitoredEvents[eventType].userData = handlingData.get();
    if (timeout > 0)
    {
        // Adding periodic task associated with socket.
        handlingData->data->timeout = timeout;
        addPeriodicTask(
            getSystemTimerVal() + timeout,
            handlingData.get()->data,
            socket,
            eventType);
    }
    handlingData.release();
}

void AioTaskQueue::removeSocketFromPollSet(Pollable* sock, aio::EventType eventType)
{
    //NX_DEBUG(this, QString::fromLatin1("removing %1, eventType %2").arg((size_t)sock, 0, 16).arg(eventType));

    void*& userData = sock->impl()->monitoredEvents[eventType].userData;
    if (userData)
        delete static_cast<AioEventHandlingDataHolder*>(userData);
    userData = nullptr;
    if (eventType == aio::etRead || eventType == aio::etWrite)
        m_pollSet->remove(sock, eventType);
}

void AioTaskQueue::processScheduledRemoveSocketTasks()
{
    processPollSetModificationQueue(TaskType::tRemoving);
}

bool AioTaskQueue::removeReverseTask(
    Pollable* const sock,
    aio::EventType eventType,
    TaskType taskType,
    AIOEventHandler* const eventHandler,
    unsigned int newTimeoutMS)
{
    for (typename std::deque<SocketAddRemoveTask>::iterator
        it = m_pollSetModificationQueue.begin();
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
            void* userData = sock->impl()->monitoredEvents[eventType].userData;
            NX_ASSERT(userData);
            static_cast<AioEventHandlingDataHolder*>(userData)->data->timeout = newTimeoutMS;
            static_cast<AioEventHandlingDataHolder*>(userData)->data->markedForRemoval.store(0);

            m_pollSetModificationQueue.erase(it);
            return true;
        }
        else if (taskType == TaskType::tRemoving && (it->type == TaskType::tAdding || it->type == TaskType::tChangingTimeout))
        {
            //if( it->type == TaskType::tChangingTimeout ) cancelling scheduled tasks, since socket removal from poll is requested
            const TaskType foundTaskType = it->type;

            //cancelling adding socket
            it = m_pollSetModificationQueue.erase(it);
            //removing futher tChangingTimeout tasks
            for (;
                it != m_pollSetModificationQueue.end();
                )
            {
                if (it->socket == sock && it->eventType == eventType)
                {
                    NX_ASSERT(it->type == TaskType::tChangingTimeout);
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

        continue;
    }

    return false;
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
            : (socket->impl()->monitoredEvents[aio::etRead].userData //in case of error calling any handler
                ? aio::etRead
                : aio::etWrite);

        //TODO #ak in case of error pollSet reports etError once,
        //but two handlers may be registered for socket.
        //So, we must notify both (if they differ, probably).
        //Currently, leaving as-is since any socket installs single handler for both events

        //TODO #ak notify second handler (if any) and if it not removed by first handler

        //NX_DEBUG(this, QString::fromLatin1("processing %1, eventType %2").arg((size_t)socket, 0, 16).arg(handlerToInvokeType));

        //no need to lock mutex, since data is removed in this thread only
        std::shared_ptr<AioEventHandlingData> handlingData =
            static_cast<AioEventHandlingDataHolder*>(
                socket->impl()->monitoredEvents[handlerToInvokeType].userData)->data;

        QnMutexLocker lk(&m_socketEventProcessingMutex);
        ++handlingData->beingProcessed;
        //TODO #ak possibly some atomic fence is required here
        if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0) //socket has been removed from watch
        {
            --handlingData->beingProcessed;
            continue;
        }
        //eventTriggered is allowed to call stopMonitoring which can remove socket from pollset
        handlingData->eventHandler->eventTriggered(socket, sockEventType);
        //updating socket's periodic task (it's garanteed that there is periodic task for socket)
        if (handlingData->timeout > 0)
            handlingData->updatedPeriodicTaskClock = curClock + handlingData->timeout;
        --handlingData->beingProcessed;
        //NOTE element, this iterator points to, could be removed in eventTriggered call,
        //but it is still safe to increment this iterator
    }
}

bool AioTaskQueue::processPeriodicTasks(const qint64 curClock)
{
    int tasksProcessedCount = 0;

    for (;;)
    {
        QnMutexLocker lk(&m_socketEventProcessingMutex);

        PeriodicTaskData periodicTaskData;
        {
            //taking task from queue
            auto it = m_periodicTasksByClock.begin();
            if (it == m_periodicTasksByClock.end() || it->first > curClock)
                break;
            periodicTaskData = it->second;
            m_periodicTasksByClock.erase(it);
        }

        // There is no need to lock mutex since data is removed in this thread only.
        auto handlingData = periodicTaskData.data; // TODO: #ak do we really need to copy shared_ptr here?
        handlingData->nextTimeoutClock = 0;
        ++handlingData->beingProcessed;
        //TODO #ak atomic fence is required here (to avoid reordering)
        //TODO #ak add some auto pointer for handlingData->beingProcessed
        if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0) //task has been removed from watch
        {
            --handlingData->beingProcessed;
            continue;
        }

        if (handlingData->updatedPeriodicTaskClock > 0)
        {
            //not processing task, but updating...
            if (handlingData->updatedPeriodicTaskClock > curClock)
            {
                //adding new task with updated clock
                addPeriodicTaskNonSafe(
                    handlingData->updatedPeriodicTaskClock,
                    handlingData,
                    periodicTaskData.socket,
                    periodicTaskData.eventType);
                handlingData->updatedPeriodicTaskClock = 0;
                --handlingData->beingProcessed;
                continue;
            }

            handlingData->updatedPeriodicTaskClock = 0;
            //updated task has to be executed now
        }
        else if (handlingData->updatedPeriodicTaskClock == -1)
        {
            //cancelling periodic task
            handlingData->updatedPeriodicTaskClock = 0;
            --handlingData->beingProcessed;
            continue;
        }

        if (periodicTaskData.socket)    //periodic event, associated with socket (e.g., socket operation timeout)
        {
            //NOTE socket is allowed to be removed in eventTriggered
            handlingData->eventHandler->eventTriggered(
                periodicTaskData.socket,
                static_cast<aio::EventType>(periodicTaskData.eventType | aio::etTimedOut));
            //adding periodic task with same timeout
            addPeriodicTaskNonSafe(
                curClock + handlingData->timeout,
                handlingData,
                periodicTaskData.socket,
                periodicTaskData.eventType);
            ++tasksProcessedCount;
        }
        //else
        //    periodicTaskData.periodicEventHandler->onTimeout( periodicTaskData.taskID );  //for periodic tasks not bound to socket
        --handlingData->beingProcessed;
    }

    return tasksProcessedCount > 0;
}

void AioTaskQueue::processPostedCalls()
{
    while (!m_postedCalls.empty())
    {
        auto postHandler = std::move(m_postedCalls.begin()->postHandler);
        m_postedCalls.erase(m_postedCalls.begin());
        postHandler();
    }
}

void AioTaskQueue::addPeriodicTask(
    const qint64 taskClock,
    const std::shared_ptr<AioEventHandlingData>& handlingData,
    Pollable* _socket,
    aio::EventType eventType)
{
    QnMutexLocker lk(&m_socketEventProcessingMutex);
    addPeriodicTaskNonSafe(taskClock, handlingData, _socket, eventType);
}

void AioTaskQueue::addPeriodicTaskNonSafe(
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

std::vector<SocketAddRemoveTask> AioTaskQueue::cancelPostedCalls(
    SocketSequenceType socketSequence)
{
    QnMutexLocker lock(&mutex);
    return cancelPostedCalls(lock, socketSequence);
}

std::vector<SocketAddRemoveTask> AioTaskQueue::cancelPostedCalls(
    const QnMutexLockerBase& /*lock*/,
    SocketSequenceType socketSequence)
{
    std::vector<SocketAddRemoveTask> elementsToRemove;

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

std::size_t AioTaskQueue::postedCallCount() const
{
    return m_postedCalls.size();
}

qint64 AioTaskQueue::nextPeriodicEventClock() const
{
    QnMutexLocker lock(&m_socketEventProcessingMutex);
    
    return m_periodicTasksByClock.empty()
        ? 0
        : m_periodicTasksByClock.cbegin()->first;
}

void AioTaskQueue::waitCurrentEventProcessingCompletion()
{
    m_socketEventProcessingMutex.lock();
    m_socketEventProcessingMutex.unlock();
}

} // namespace nx::network::aio::detail
