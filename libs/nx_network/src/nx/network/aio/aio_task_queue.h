// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <optional>

#include <nx/utils/math/abnormal_value_detector.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_pollset.h"
#include "aio_event_handler.h"
#include "pollable.h"
#include "../common_socket_impl.h"
#include "../detail/socket_sequence.h"

namespace nx::network::aio::detail {

//-------------------------------------------------------------------------------------------------

// TODO: #akolesnikov Looks like a flags set, but somehow it is not.
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
    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero();
    qint64 updatedPeriodicTaskClock = 0;
    /** Clock when timer will be triggered. 0 - no clock. */
    qint64 nextTimeoutClock = 0;

    AioEventHandlingData(AIOEventHandler* _eventHandler):
        eventHandler(_eventHandler)
    {
    }
};

// TODO: #akolesnikov It makes sense to split this class to multiple ones containing only desired data.
class SocketAddRemoveTask
{
public:
    TaskType type;
    Pollable* socket;
    /** Socket number that is still unique after socket has been destroyed. */
    SocketSequence socketSequence;
    aio::EventType eventType;
    AIOEventHandler* eventHandler;
    /** 0 means no timeout. */
    std::chrono::milliseconds timeout;
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
        std::chrono::milliseconds _timeout = std::chrono::milliseconds::zero(),
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
            std::chrono::milliseconds::zero(),
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
        SocketSequence socketSequence,
        std::atomic<int>* const _taskCompletionEvent = nullptr)
        :
        SocketAddRemoveTask(
            TaskType::tCancelPostedCalls,
            nullptr,
            aio::etNone,
            nullptr,
            std::chrono::milliseconds::zero(),
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
 * TODO: #akolesnikov Should split this class to multiple clear classes with a single responsibility.
 */
class NX_NETWORK_API AioTaskQueue
{
public:
    AioTaskQueue(AbstractPollSet* pollSet);

    //---------------------------------------------------------------------------------------------
    // Methods that are called within any thread.

    void addTask(SocketAddRemoveTask task);

    bool taskExists(
        Pollable* const sock,
        aio::EventType eventType,
        TaskType taskType) const;

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
        std::chrono::milliseconds newTimeout);

    std::size_t newReadMonitorTaskCount() const;
    std::size_t newWriteMonitorTaskCount() const;

    qint64 nextPeriodicEventClock() const;

    std::size_t periodicTasksCount() const;

    void clear();

    bool empty() const;

    //---------------------------------------------------------------------------------------------
    // Methods that are called within the corresponding AIO thread only.

    void processPollSetModificationQueue(TaskType taskFilter);
    void processScheduledRemoveSocketTasks();

    /** Processes events from pollSet. */
    void processSocketEvents(const qint64 curClock);

    /**
     * @return true, if at least one task has been processed.
     */
    bool processPeriodicTasks(const qint64 curClock);

    void processPostedCalls();

    /**
     * TODO: Review publicMutex requirements.
     */
    void removeSocketFromPollSet(Pollable* sock, aio::EventType eventType);

    /**
     * Moves elements to remove to a temporary container and returns it.
     * Elements may contain functor which may contain aio objects (sockets) which will be removed
     * when removing functor. This may lead to a deadlock if we not release lock.
     */
    std::vector<SocketAddRemoveTask> cancelPostedCalls(
        SocketSequence socketSequence);

    std::size_t postedCallCount() const;

    //---------------------------------------------------------------------------------------------

    /**
     * Used as a clock for periodic events. Function introduced since implementation can be changed.
     * @return Milliseconds since some unspecified system event.
     */
    static qint64 getMonotonicTime();

private:
    AbstractPollSet* m_pollSet = nullptr;
    // TODO #akolesnikov: Use cyclic array here to minimize allocations.
    /**
     * NOTE: This variable can be accessed within aio thread only.
     */
    std::deque<SocketAddRemoveTask> m_postedCalls;
    std::deque<SocketAddRemoveTask> m_pollSetModificationQueue;
    // TODO: #akolesnikov Get rid of map here to avoid undesired allocations.
    std::multimap<qint64, PeriodicTaskData> m_periodicTasksByClock;
    mutable nx::Mutex m_mutex;
    nx::utils::math::AbnormalValueDetector<
        std::chrono::microseconds, int, const char*> m_abnormalProcessingTimeDetector;
    std::atomic<std::size_t> m_newReadMonitorTaskCount = 0;
    std::atomic<std::size_t> m_newWriteMonitorTaskCount = 0;

    void processAddTask(
        const nx::Locker<nx::Mutex>& lock,
        SocketAddRemoveTask& task);

    void processChangeTimeoutTask(
        const nx::Locker<nx::Mutex>& lock,
        SocketAddRemoveTask& task);

    void processRemoveTask(
        const nx::Locker<nx::Mutex>& lock,
        SocketAddRemoveTask& task);

    void processCallFuncTask(
        const nx::Locker<nx::Mutex>& lock,
        SocketAddRemoveTask& task);

    std::vector<SocketAddRemoveTask> processCancelPostedCallTask(
        const nx::Locker<nx::Mutex>& lock,
        SocketAddRemoveTask& task);

    //---------------------------------------------------------------------------------------------

    void addSocketToPollset(
        const nx::Locker<nx::Mutex>& lock,
        Pollable* socket,
        aio::EventType eventType,
        std::chrono::milliseconds timeout,
        AIOEventHandler* eventHandler);

    void removeSocketFromPollSet(
        const nx::Locker<nx::Mutex>& lock,
        Pollable* sock,
        aio::EventType eventType);

    //---------------------------------------------------------------------------------------------

    void addPeriodicTask(
        const nx::Locker<nx::Mutex>& lock,
        const qint64 taskClock,
        const std::shared_ptr<AioEventHandlingData>& handlingData,
        Pollable* _socket,
        aio::EventType eventType);

    std::optional<PeriodicTaskData> takeNextExpiredPeriodicTask(qint64 curClock);

    void replacePeriodicTask(
        const nx::Locker<nx::Mutex>& lock,
        const std::shared_ptr<AioEventHandlingData>& handlingData,
        qint64 newClock,
        Pollable* socket,
        aio::EventType eventType);

    void cancelPeriodicTask(
        const nx::Locker<nx::Mutex>& /*lock*/,
        AioEventHandlingData* eventHandlingData,
        aio::EventType eventType);

    //---------------------------------------------------------------------------------------------

    std::vector<SocketAddRemoveTask> cancelPostedCalls(
        const nx::Locker<nx::Mutex>& /*lock*/,
        SocketSequence socketSequence);

    template<typename Func>
    void callAndReportAbnormalProcessingTime(Func func, const char* description);

    void reportAbnormalProcessingTime(
        std::chrono::microseconds value,
        std::chrono::microseconds average,
        const char* where_);
};

} // namespace nx::network::aio::detail
