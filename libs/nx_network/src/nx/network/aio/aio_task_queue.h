#pragma once

#include <atomic>
#include <deque>
#include <memory>

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>

#include "abstract_pollset.h"
#include "aio_event_handler.h"
#include "pollable.h"
#include "../detail/socket_sequence.h"

namespace nx::network::aio::detail {

// TODO: #ak Looks like a flags set, but somehow it is not.
enum class TaskType
{
    tAdding,
    tChangingTimeout,
    tRemoving,
    /** Call functor in aio thread. */
    tCallFunc,
    /** Cancel tCallFunc tasks. */
    tCancelPostedCalls,
    tAll
};

/**
 * Used as userdata in PollSet. One AioEventHandlingData object corresponds to pair (socket, eventType).
 */
class AioEventHandlingData
{
public:
    std::atomic<int> beingProcessed{0};
    std::atomic<int> markedForRemoval{0};
    AIOEventHandler* eventHandler = nullptr;
    /** 0 means no timeout. */
    unsigned int timeout = 0;
    qint64 updatedPeriodicTaskClock = 0;
    /** Clock when timer will be triggered. 0 - no clock. */
    qint64 nextTimeoutClock = 0;

    AioEventHandlingData(AIOEventHandler* _eventHandler):
        eventHandler(_eventHandler)
    {
    }
};

// TODO: #ak It makes sense to split this class to multiple ones containing only desired data.
class SocketAddRemoveTask
{
public:
    TaskType type;
    Pollable* socket;
    /** Socket number that is still unique after socket has been destroyed. */
    SocketSequenceType socketSequence;
    aio::EventType eventType;
    AIOEventHandler* eventHandler;
    /** 0 means no timeout. */
    unsigned int timeout;
    std::atomic<int>* taskCompletionEvent;
    nx::utils::MoveOnlyFunc<void()> postHandler;
    nx::utils::MoveOnlyFunc<void()> taskCompletionHandler;

    /**
     * @param taskCompletionEvent if not NULL, set to 1 after processing task.
     */
    SocketAddRemoveTask(
        TaskType _type,
        Pollable* const _socket,
        aio::EventType _eventType,
        AIOEventHandler* const _eventHandler,
        unsigned int _timeout = 0,
        std::atomic<int>* const _taskCompletionEvent = nullptr,
        nx::utils::MoveOnlyFunc<void()> _taskCompletionHandler = nx::utils::MoveOnlyFunc<void()>())
        :
        type(_type),
        socket(_socket),
        socketSequence(_socket ? _socket->impl()->socketSequence : 0),
        eventType(_eventType),
        eventHandler(_eventHandler),
        timeout(_timeout),
        taskCompletionEvent(_taskCompletionEvent),
        taskCompletionHandler(std::move(_taskCompletionHandler))
    {
    }

    SocketAddRemoveTask(SocketAddRemoveTask&&) = default;
    SocketAddRemoveTask& operator=(SocketAddRemoveTask&&) = default;
};

class PostAsyncCallTask:
    public SocketAddRemoveTask
{
public:
    PostAsyncCallTask(
        Pollable* const _socket,
        nx::utils::MoveOnlyFunc<void()> _postHandler)
        :
        SocketAddRemoveTask(
            TaskType::tCallFunc,
            _socket,
            aio::etNone,
            nullptr,
            0,
            nullptr)
    {
        this->postHandler = std::move(_postHandler);
        if (_socket)
            this->socketSequence = _socket->impl()->socketSequence;
    }
};

class CancelPostedCallsTask:
    public SocketAddRemoveTask
{
public:
    CancelPostedCallsTask(
        SocketSequenceType socketSequence,
        std::atomic<int>* const _taskCompletionEvent = nullptr)
        :
        SocketAddRemoveTask(
            TaskType::tCancelPostedCalls,
            nullptr,
            aio::etNone,
            nullptr,
            0,
            _taskCompletionEvent)
    {
        this->socketSequence = socketSequence;
    }
};

class PeriodicTaskData
{
public:
    std::shared_ptr<AioEventHandlingData> data;
    Pollable* socket = nullptr;
    aio::EventType eventType = aio::etNone;

    PeriodicTaskData() = default;

    PeriodicTaskData(
        const std::shared_ptr<AioEventHandlingData>& _data,
        Pollable* _socket,
        aio::EventType _eventType)
        :
        data(_data),
        socket(_socket),
        eventType(_eventType)
    {
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * This class used to be AioThread private impl class,
 * so it contains different and unrelated data.
 * TODO: #ak Should split this class to multiple clear classes with a single responsibility.
 */
class NX_NETWORK_API AioTaskQueue
{
public:
    // TODO: #ak Move all these variables to the private section.
    // TODO: #ak Too many mutexes here. Refactoring required.

    unsigned int newReadMonitorTaskCount = 0;
    unsigned int newWriteMonitorTaskCount = 0;
    // Used to make AioThread public API thread-safe (to serialize access to internal structures).
    mutable QnMutex mutex;

    AioTaskQueue(AbstractPollSet* pollSet);

    /**
     * Used as a clock for periodic events. Function introduced since implementation can be changed.
     */
    qint64 getSystemTimerVal() const;

    void addTask(SocketAddRemoveTask task);

    bool taskExists(
        Pollable* const sock,
        aio::EventType eventType,
        TaskType taskType) const;

    void postAsyncCall(Pollable* const pollable, nx::utils::MoveOnlyFunc<void()> func);

    void processPollSetModificationQueue(TaskType taskFilter);
    void removeSocketFromPollSet(Pollable* sock, aio::EventType eventType);
    void processScheduledRemoveSocketTasks();

    /**
     * This method introduced for optimization: if we fast call startMonitoring then removeSocket
     * (socket has not been added to pollset yet), then removeSocket can just cancel
     * "add socket to pollset" task. And vice versa.
     * @return true if reverse task has been cancelled and socket
     *   is already in desired state, no futher processing is needed.
     */
    bool removeReverseTask(
        Pollable* const sock,
        aio::EventType eventType,
        TaskType taskType,
        AIOEventHandler* const eventHandler,
        unsigned int newTimeoutMS);

    /** Processes events from pollSet. */
    void processSocketEvents(const qint64 curClock);
    
    /**
     * @return true, if at least one task has been processed.
     */
    bool processPeriodicTasks(const qint64 curClock);
    void processPostedCalls();
    
    /**
     * Moves elements to remove to a temporary container and returns it.
     * Elements may contain functor which may contain aio objects (sockets) which will be removed
     * when removing functor. This may lead to a dead lock if we not release lock.
     */
    std::vector<SocketAddRemoveTask> cancelPostedCalls(
        SocketSequenceType socketSequence);
    
    std::vector<SocketAddRemoveTask> cancelPostedCalls(
        const QnMutexLockerBase& /*lock*/,
        SocketSequenceType socketSequence);

    std::size_t postedCallCount() const;

    qint64 nextPeriodicEventClock() const;

    void waitCurrentEventProcessingCompletion();

    std::size_t periodicTasksCount() const;

private:
    AbstractPollSet* m_pollSet = nullptr;
    QElapsedTimer m_monotonicClock;
    // TODO #ak: Use cyclic array here to minimize allocations.
    /**
     * NOTE: This variable can be accessed within aio thread only.
     */
    std::deque<SocketAddRemoveTask> m_postedCalls;
    std::deque<SocketAddRemoveTask> m_pollSetModificationQueue;
    // TODO: #ak Get rid of map here to avoid undesired allocations.
    std::multimap<qint64, PeriodicTaskData> m_periodicTasksByClock;
    mutable QnMutex m_socketEventProcessingMutex;

    void addSocketToPollset(
        Pollable* socket,
        aio::EventType eventType,
        int timeout,
        AIOEventHandler* eventHandler);

    void addPeriodicTask(
        const qint64 taskClock,
        const std::shared_ptr<AioEventHandlingData>& handlingData,
        Pollable* _socket,
        aio::EventType eventType);

    void addPeriodicTaskNonSafe(
        const qint64 taskClock,
        const std::shared_ptr<AioEventHandlingData>& handlingData,
        Pollable* _socket,
        aio::EventType eventType);

    void cancelPeriodicTask(
        AioEventHandlingData* eventHandlingData,
        aio::EventType eventType);
};

} // namespace nx::network::aio::detail
