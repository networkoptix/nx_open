/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aio_thread_impl.h"

#include <nx/utils/log/log.h>


namespace nx {
namespace network {
namespace aio {
namespace detail {

AIOThreadImpl::AIOThreadImpl()
:
    newReadMonitorTaskCount(0),
    newWriteMonitorTaskCount(0),
    processingPostedCalls(0)
{
    m_monotonicClock.restart();
}

//!used as clock for periodic events. Function introduced since implementation can be changed
qint64 AIOThreadImpl::getSystemTimerVal() const
{
    return m_monotonicClock.elapsed();
}

void AIOThreadImpl::processPollSetModificationQueue(TaskType taskFilter)
{
    std::vector<SocketAddRemoveTask> elementsToRemove;
    QnMutexLocker lk(&mutex);

    for (typename std::deque<SocketAddRemoveTask>::iterator
        it = pollSetModificationQueue.begin();
        it != pollSetModificationQueue.end();
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
                addSockToPollset(
                    task.socket,
                    task.eventType,
                    task.timeout,
                    task.eventHandler);
                break;
            }

            case TaskType::tChangingTimeout:
            {
                void* userData = task.socket->impl()->eventTypeToUserData[task.eventType];
                AIOEventHandlingDataHolder* handlingData =
                    reinterpret_cast<AIOEventHandlingDataHolder*>(userData);
                //NOTE we are in aio thread currently
                if (task.timeout > 0)
                {
                    //adding/updating periodic task
                    if (handlingData->data->timeout > 0)
                    {
                        //updating existing task
                        const auto newClock = getSystemTimerVal() + task.timeout;
                        if (handlingData->data->nextTimeoutClock == 0)
                        {
                            handlingData->data->updatedPeriodicTaskClock = newClock;
                        }
                        else
                        {
                            //replacing timer record in periodicTasksByClock
                            for (auto it = periodicTasksByClock.lower_bound(handlingData->data->nextTimeoutClock);
                                    it != periodicTasksByClock.end() && it->first == handlingData->data->nextTimeoutClock;
                                    ++it)
                            {
                                if (it->second.socket == task.socket)
                                {
                                    periodicTasksByClock.erase(it);
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
                    handlingData->data->updatedPeriodicTaskClock = -1;  //cancelling existing periodic task (there must be one)
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
                postedCalls.push_back(std::move(task));
                //this task differs from every else in a way that it is not processed here, 
                    //just moved to another container. TODO #ak is it really needed to move to another container?
                it = pollSetModificationQueue.erase(it);
                continue;
            }

            case TaskType::tCancelPostedCalls:
            {
                auto postedCallsToRemove = cancelPostedCallsInternal(&lk, task.socketSequence);
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
        it = pollSetModificationQueue.erase(it);
    }
}

void AIOThreadImpl::addSockToPollset(
    Pollable* socket,
    aio::EventType eventType,
    int timeout,
    AIOEventHandler<Pollable>* eventHandler)
{
    std::unique_ptr<AIOEventHandlingDataHolder> handlingData(new AIOEventHandlingDataHolder(eventHandler));
    bool failedToAddToPollset = false;
    if (eventType != aio::etTimedOut)
    {
        if (!pollSet.add(socket, eventType, handlingData.get()))
        {
            const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
            NX_LOG(QString::fromLatin1("Failed to add socket to pollset. %1").arg(SystemError::toString(errorCode)), cl_logWARNING);
            failedToAddToPollset = true;
        }
    }
    socket->impl()->eventTypeToUserData[eventType] = handlingData.get();

    if (failedToAddToPollset)
    {
        postedCalls.push_back(
            PostAsyncCallTask(
                socket,
                [eventHandler, socket](){
                    eventHandler->eventTriggered(socket, aio::etError);
                }));
    }
    else if (timeout > 0)
    {
        //adding periodic task associated with socket
        handlingData->data->timeout = timeout;
        addPeriodicTask(
            getSystemTimerVal() + timeout,
            handlingData.get()->data,
            socket,
            eventType);
    }
    handlingData.release();
}

void AIOThreadImpl::removeSocketFromPollSet(Pollable* sock, aio::EventType eventType)
{
    //NX_LOG( QString::fromLatin1("removing %1, eventType %2").arg((size_t)sock, 0, 16).arg(eventType), cl_logDEBUG1 );

    void*& userData = sock->impl()->eventTypeToUserData[eventType];
    if (userData)
        delete static_cast<AIOEventHandlingDataHolder*>(userData);
    userData = nullptr;
    if (eventType == aio::etRead || eventType == aio::etWrite)
        pollSet.remove(sock, eventType);
}

void AIOThreadImpl::removeSocketsFromPollSet()
{
    processPollSetModificationQueue(TaskType::tRemoving);
}

/*!
    This method introduced for optimization: if we fast call watchSocket then removeSocket (socket has not been added to pollset yet),
    than removeSocket can just cancel watchSocket task. And vice versa
    \return \a true if reverse task has been cancelled and socket is already in desired state, no futher processing is needed
*/
bool AIOThreadImpl::removeReverseTask(
    Pollable* const sock,
    aio::EventType eventType,
    TaskType taskType,
    AIOEventHandler<Pollable>* const eventHandler,
    unsigned int newTimeoutMS)
{
    Q_UNUSED(eventHandler)

    for (typename std::deque<SocketAddRemoveTask>::iterator
        it = pollSetModificationQueue.begin();
        it != pollSetModificationQueue.end();
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
            void* userData = sock->impl()->eventTypeToUserData[eventType];
            NX_ASSERT(userData);
            static_cast<AIOEventHandlingDataHolder*>(userData)->data->timeout = newTimeoutMS;
            static_cast<AIOEventHandlingDataHolder*>(userData)->data->markedForRemoval.store(0);

            pollSetModificationQueue.erase(it);
            return true;
        }
        else if (taskType == TaskType::tRemoving && (it->type == TaskType::tAdding || it->type == TaskType::tChangingTimeout))
        {
            //if( it->type == TaskType::tChangingTimeout ) cancelling scheduled tasks, since socket removal from poll is requested
            const TaskType foundTaskType = it->type;

            //cancelling adding socket
            it = pollSetModificationQueue.erase(it);
            //removing futher tChangingTimeout tasks
            for (;
            it != pollSetModificationQueue.end();
                ++it)
            {
                if (it->socket == sock && it->eventType == eventType)
                {
                    NX_ASSERT(it->type == TaskType::tChangingTimeout);
                    it = pollSetModificationQueue.erase(it);
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

//!Processes events from \a pollSet
void AIOThreadImpl::processSocketEvents(const qint64 curClock)
{
    for (typename UnifiedPollSet::const_iterator
        it = pollSet.begin();
        it != pollSet.end();
        )
    {
        Pollable* const socket = it.socket();
        const aio::EventType sockEventType = it.eventType();
        const aio::EventType handlerToInvokeType =
            (sockEventType == aio::etRead || sockEventType == aio::etWrite)
            ? sockEventType
            : (socket->impl()->eventTypeToUserData[aio::etRead] //in case of error calling any handler
                ? aio::etRead
                : aio::etWrite);

        //TODO #ak in case of error pollSet reports etError once, 
        //but two handlers may be registered for socket.
        //So, we must notify both (if they differ, probably). 
        //Currently, leaving as-is since any socket installs single handler for both events

        //TODO #ak notify second handler (if any) and if it not removed by first handler

        //NX_LOG( QString::fromLatin1("processing %1, eventType %2").arg((size_t)socket, 0, 16).arg(handlerToInvokeType), cl_logDEBUG1 );

        //no need to lock mutex, since data is removed in this thread only
        std::shared_ptr<AIOEventHandlingData> handlingData =
            static_cast<AIOEventHandlingDataHolder*>(
                socket->impl()->eventTypeToUserData[handlerToInvokeType])->data;

        QnMutexLocker lk(&socketEventProcessingMutex);
        ++handlingData->beingProcessed;
        //TODO #ak possibly some atomic fence is required here
        if (handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0) //socket has been removed from watch
        {
            --handlingData->beingProcessed;
            ++it;
            continue;
        }
        //eventTriggered is allowed to call removeFromWatch which can remove socket from pollset
        handlingData->eventHandler->eventTriggered(socket, sockEventType);
        //updating socket's periodic task (it's garanteed that there is periodic task for socket)
        if (handlingData->timeout > 0)
            handlingData->updatedPeriodicTaskClock = curClock + handlingData->timeout;
        --handlingData->beingProcessed;
        //NOTE element, this iterator points to, could be removed in eventTriggered call, 
        //but it is still safe to increment this iterator
        ++it;
    }
}

/*!
    \return \a true, if at least one task has been processed
*/
bool AIOThreadImpl::processPeriodicTasks(const qint64 curClock)
{
    int tasksProcessedCount = 0;

    for (;; )
    {
        QnMutexLocker lk(&socketEventProcessingMutex);

        PeriodicTaskData periodicTaskData;
        {
            //taking task from queue
            typename std::multimap<qint64, PeriodicTaskData>::iterator it = periodicTasksByClock.begin();
            if (it == periodicTasksByClock.end() || it->first > curClock)
                break;
            periodicTaskData = it->second;
            periodicTasksByClock.erase(it);
        }

        //no need to lock mutex, since data is removed in this thread only
        std::shared_ptr<AIOEventHandlingData> handlingData = periodicTaskData.data; //TODO #ak do we really need to copy shared_ptr here?
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

void AIOThreadImpl::processPostedCalls()
{
    while (!postedCalls.empty())
    {
        auto postHandler = std::move(postedCalls.begin()->postHandler);
        postedCalls.erase(postedCalls.begin());
        postHandler();
    }
}

void AIOThreadImpl::addPeriodicTask(
    const qint64 taskClock,
    const std::shared_ptr<AIOEventHandlingData>& handlingData,
    Pollable* _socket,
    aio::EventType eventType)
{
    QnMutexLocker lk(&socketEventProcessingMutex);
    addPeriodicTaskNonSafe(taskClock, handlingData, _socket, eventType);
}

void AIOThreadImpl::addPeriodicTaskNonSafe(
    const qint64 taskClock,
    const std::shared_ptr<AIOEventHandlingData>& handlingData,
    Pollable* _socket,
    aio::EventType eventType)
{
    handlingData->nextTimeoutClock = taskClock;
    periodicTasksByClock.insert(std::make_pair(
        taskClock,
        PeriodicTaskData(handlingData, _socket, eventType)));
}

/** Moves elements to remove to a temporary container and returns it.
    Elements may contain functor which may contain aio objects (sockets) which will be remove
    when removing functor. This may lead to a dead lock if we not release \a lock
    */
std::vector<AIOThreadImpl::SocketAddRemoveTask> AIOThreadImpl::cancelPostedCallsInternal(
    QnMutexLockerBase* const /*lock*/,
    SocketSequenceType socketSequence)
{
    //for (typename std::deque<SocketAddRemoveTask>::iterator
    //    it = pollSetModificationQueue.begin();
    //    it != pollSetModificationQueue.end();
    //    )
    //{
    //    if (it->type == TaskType::tCallFunc && it->socketSequence == socketSequence)
    //        it = pollSetModificationQueue.erase(it);
    //    else
    //        ++it;
    //}

    //for (typename std::deque<SocketAddRemoveTask>::iterator
    //    it = postedCalls.begin();
    //    it != postedCalls.end();
    //    )
    //{
    //    if (it->socketSequence == socketSequence)
    //        it = postedCalls.erase(it);
    //    else
    //        ++it;
    //}

    //detecting range of elements to remove
    const auto tasksToRemoveRangeStart = std::remove_if(
        pollSetModificationQueue.begin(),
        pollSetModificationQueue.end(),
        [socketSequence](const SocketAddRemoveTask& val)
        {
            return val.type == TaskType::tCallFunc &&
                    val.socketSequence == socketSequence;
        });

    const auto postedCallsRemoveRangeStart = std::remove_if(
        postedCalls.begin(),
        postedCalls.end(),
        [socketSequence](const SocketAddRemoveTask& val)
        {
            return val.socketSequence == socketSequence;
        });

    //moving elements to remove to local container
    std::vector<SocketAddRemoveTask> elementsToRemove;
    elementsToRemove.reserve(
        std::distance(tasksToRemoveRangeStart, pollSetModificationQueue.end())+
        std::distance(postedCallsRemoveRangeStart, postedCalls.end()));

    auto elementsToRemoveInserter = std::back_inserter(elementsToRemove);
    for (auto it = tasksToRemoveRangeStart; it != pollSetModificationQueue.end(); ++it)
        elementsToRemoveInserter = std::move(*it);
    for (auto it = postedCallsRemoveRangeStart; it != postedCalls.end(); ++it)
        elementsToRemoveInserter = std::move(*it);

    //removing elements from source container
    pollSetModificationQueue.erase(
        tasksToRemoveRangeStart,
        pollSetModificationQueue.end());
    postedCalls.erase(
        postedCallsRemoveRangeStart,
        postedCalls.end());

    return elementsToRemove;
}

}   //detail
}   //aio
}   //network
}   //nx
