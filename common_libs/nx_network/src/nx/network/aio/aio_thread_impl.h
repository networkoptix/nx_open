/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#pragma once

#include <atomic>
#include <deque>
#include <memory>

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>

#include "aioeventhandler.h"
#include "unified_pollset.h"
#include "../detail/socket_sequence.h"


namespace nx {
namespace network {
namespace aio {
namespace detail {

enum class TaskType
{
    tAdding,
    tChangingTimeout,
    tRemoving,
    //!Call functor in aio thread
    tCallFunc,
    //!Cancel \a tCallFunc tasks
    tCancelPostedCalls,
    tAll
};

//!Used as userdata in PollSet. One \a AIOEventHandlingData object corresponds to pair (\a socket, \a eventType)
class AIOEventHandlingData
{
public:
    std::atomic<int> beingProcessed;
    std::atomic<int> markedForRemoval;
    AIOEventHandler<Pollable>* eventHandler;
    //!0 means no timeout
    unsigned int timeout;
    qint64 updatedPeriodicTaskClock;
    /** clock when timer will be triggered. 0 - no clock */
    qint64 nextTimeoutClock;

    AIOEventHandlingData(AIOEventHandler<Pollable>* _eventHandler)
    :
        beingProcessed(0),
        markedForRemoval(0),
        eventHandler(_eventHandler),
        timeout(0),
        updatedPeriodicTaskClock(0),
        nextTimeoutClock(0)
    {
    }
};

class AIOEventHandlingDataHolder
{
public:
    //!Why the fuck do we need shared_ptr inside object instanciated on heap?
    std::shared_ptr<AIOEventHandlingData> data;

    AIOEventHandlingDataHolder(AIOEventHandler<Pollable>* _eventHandler)
    :
        data(new AIOEventHandlingData(_eventHandler))
    {
    }
};

class AIOThreadImpl
{
public:
    //!TODO #ak better to split this class to multiple ones containing only desired data
    class SocketAddRemoveTask
    {
    public:
        TaskType type;
        Pollable* socket;
        //!Socket number that is still unique after socket has been destroyed
        SocketSequenceType socketSequence;
        aio::EventType eventType;
        AIOEventHandler<Pollable>* eventHandler;
        //!0 means no timeout
        unsigned int timeout;
        std::atomic<int>* taskCompletionEvent;
        nx::utils::MoveOnlyFunc<void()> postHandler;
        nx::utils::MoveOnlyFunc<void()> taskCompletionHandler;

        /*!
            \param taskCompletionEvent if not NULL, set to 1 after processing task
        */
        SocketAddRemoveTask(
            TaskType _type,
            Pollable* const _socket,
            aio::EventType _eventType,
            AIOEventHandler<Pollable>* const _eventHandler,
            unsigned int _timeout = 0,
            std::atomic<int>* const _taskCompletionEvent = nullptr,
            nx::utils::MoveOnlyFunc<void()> _taskCompletionHandler = nx::utils::MoveOnlyFunc<void()>())
        :
            type(_type),
            socket(_socket),
            socketSequence(0),
            eventType(_eventType),
            eventHandler(_eventHandler),
            timeout(_timeout),
            taskCompletionEvent(_taskCompletionEvent),
            taskCompletionHandler(std::move(_taskCompletionHandler))
        {
        }
    };

    class PostAsyncCallTask
    :
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

    class CancelPostedCallsTask
    :
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
        std::shared_ptr<AIOEventHandlingData> data;
        Pollable* socket;
        aio::EventType eventType;

        PeriodicTaskData()
        :
            socket(NULL),
            eventType(aio::etNone)
        {
        }

        PeriodicTaskData(
            const std::shared_ptr<AIOEventHandlingData>& _data,
            Pollable* _socket,
            aio::EventType _eventType)
        :
            data(_data),
            socket(_socket),
            eventType(_eventType)
        {
        }
    };

    //typedef typename socket_to_pollset_static_map::get<Pollable>::value UnifiedPollSet;

    //TODO #ak too many mutexes here. Refactoring required

    UnifiedPollSet pollSet;
    std::deque<SocketAddRemoveTask> pollSetModificationQueue;
    unsigned int newReadMonitorTaskCount;
    unsigned int newWriteMonitorTaskCount;
    /** used to make public API thread-safe (to serialize access to internal structures) */
    mutable QnMutex mutex;
    mutable QnMutex socketEventProcessingMutex;
    //TODO #ak get rid of map here to avoid undesired allocations
    std::multimap<qint64, PeriodicTaskData> periodicTasksByClock;
    //TODO #ak use cyclic array here to minimize allocations
    /*!
        \note This variable is accessed within aio thread only
    */
    std::deque<SocketAddRemoveTask> postedCalls;
    std::atomic<int> processingPostedCalls;

    AIOThreadImpl();

    //!used as clock for periodic events. Function introduced since implementation can be changed
    qint64 getSystemTimerVal() const;
    void processPollSetModificationQueue(TaskType taskFilter);
    void addSockToPollset(
        Pollable* socket,
        aio::EventType eventType,
        int timeout,
        AIOEventHandler<Pollable>* eventHandler);
    void removeSocketFromPollSet(Pollable* sock, aio::EventType eventType);
    void removeSocketsFromPollSet();
    /*!
        This method introduced for optimization: if we fast call watchSocket then removeSocket (socket has not been added to pollset yet),
        than removeSocket can just cancel watchSocket task. And vice versa
        \return \a true if reverse task has been cancelled and socket is already in desired state, no futher processing is needed
    */
    bool removeReverseTask(
        Pollable* const sock,
        aio::EventType eventType,
        TaskType taskType,
        AIOEventHandler<Pollable>* const eventHandler,
        unsigned int newTimeoutMS);
    //!Processes events from \a pollSet
    void processSocketEvents(const qint64 curClock);
    /*!
        \return \a true, if at least one task has been processed
    */
    bool processPeriodicTasks(const qint64 curClock);
    void processPostedCalls();
    void addPeriodicTask(
        const qint64 taskClock,
        const std::shared_ptr<AIOEventHandlingData>& handlingData,
        Pollable* _socket,
        aio::EventType eventType);
    void addPeriodicTaskNonSafe(
        const qint64 taskClock,
        const std::shared_ptr<AIOEventHandlingData>& handlingData,
        Pollable* _socket,
        aio::EventType eventType);
    /** Moves elements to remove to a temporary container and returns it.
        Elements may contain functor which may contain aio objects (sockets) which will be remove
        when removing functor. This may lead to a dead lock if we not release \a lock
     */
    std::vector<SocketAddRemoveTask> cancelPostedCallsInternal(
        QnMutexLockerBase* const /*lock*/,
        SocketSequenceType socketSequence);

private:
    QElapsedTimer m_monotonicClock;
};

}   //detail
}   //aio
}   //network
}   //nx
