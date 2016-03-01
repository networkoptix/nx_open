/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOTHREAD_H
#define AIOTHREAD_H

#include <atomic>
#include <deque>
#include <memory>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_util.h>
#include <utils/common/long_runnable.h>
#include <nx/utils/log/log.h>
#include <utils/common/systemerror.h>

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QString>

#include "aioeventhandler.h"
#include "pollset.h"
#include "unified_pollset.h"
#include "../detail/socket_sequence.h"
#include "../system_socket.h"
#include "../udt/udt_socket.h"
//#include "../udt/udt_pollset.h"


namespace nx {
namespace network {
namespace aio {
namespace detail {

template<class SocketType, class PollSetType> class AIOThreadImpl;

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
template<class SocketType>
class AIOEventHandlingData
{
public:
    std::atomic<int> beingProcessed;
    std::atomic<int> markedForRemoval;
    AIOEventHandler<SocketType>* eventHandler;
    //!0 means no timeout
    unsigned int timeout;
    qint64 updatedPeriodicTaskClock;

    AIOEventHandlingData(AIOEventHandler<SocketType>* _eventHandler)
    :
        beingProcessed(0),
        markedForRemoval(0),
        eventHandler(_eventHandler),
        timeout(0),
        updatedPeriodicTaskClock(0)
    {
    }
};

template<class SocketType>
class AIOEventHandlingDataHolder
{
public:
    //!Why the fuck do we need shared_ptr inside object instanciated on heap?
    std::shared_ptr<AIOEventHandlingData<SocketType>> data;

    AIOEventHandlingDataHolder(AIOEventHandler<SocketType>* _eventHandler)
    :
        data(new AIOEventHandlingData<SocketType>(_eventHandler))
    {
    }
};

/*!
    This class is intended for use only with aio::AIOService
    \todo make it nested in aio::AIOService?
    \note All methods, except for \a pleaseStop(), must be called with \a mutex locked
*/
template<class SocketType, class PollSetType>
class AIOThread
:
    public QnLongRunnable
{
public:
    /*!
        \param mutex Mutex to use for exclusive access to internal data
    */
    AIOThread()
    :
        QnLongRunnable( false ),
        m_impl( new AIOThreadImplType() )
    {
        setObjectName(QString::fromLatin1("AIOThread") );
    }
    virtual ~AIOThread()
    {
        pleaseStop();
        wait();

        delete m_impl;
        m_impl = NULL;
    }

    //!Implementation of QnLongRunnable::pleaseStop
    virtual void pleaseStop()
    {
        QnLongRunnable::pleaseStop();
        m_impl->pollSet.interrupt();
    }

    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \note MUST be called with \a mutex locked
    */
    void watchSocket(
        SocketType* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<SocketType>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(),
        nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler = nx::utils::MoveOnlyFunc<void()>() )
    {
        QnMutexLocker lk(&m_impl->mutex);

        //checking queue for reverse task for \a sock
        if (m_impl->removeReverseTask(sock, eventToWatch, TaskType::tAdding, eventHandler, timeoutMs.count()))
            return;    //ignoring task

        m_impl->pollSetModificationQueue.push_back(typename AIOThreadImplType::SocketAddRemoveTask(
            TaskType::tAdding,
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
    void changeSocketTimeout(
        SocketType* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<SocketType>* const eventHandler,
        std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(0),
        std::function<void()> socketAddedToPollHandler = std::function<void()>() )
    {
        QnMutexLocker lk(&m_impl->mutex);
        //TODO #ak looks like copy-paste of previous method. Remove copy-paste nahuy!!!

        //this task does not cancel any other task. TODO #ak maybe it should cancel another timeout change?
        ////checking queue for reverse task for \a sock
        //if( m_impl->removeReverseTask( sock, eventToWatch, TaskType::tAdding, eventHandler, timeoutMs ) )
        //    return true;    //ignoring task

        //if socket is marked for removal, not adding task
        void* userData = sock->impl()->eventTypeToUserData[eventToWatch];
        NX_ASSERT(userData != nullptr);  //socket is not polled, but someone wants to change timeout
        if (static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->markedForRemoval.load(std::memory_order_relaxed) > 0)
            return;   //socket marked for removal, ignoring timeout change (like, cancelling it right now)

        m_impl->pollSetModificationQueue.push_back(typename AIOThreadImplType::SocketAddRemoveTask(
            TaskType::tChangingTimeout,
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
    void removeFromWatch(
        SocketType* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion,
        nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler = nx::utils::MoveOnlyFunc<void()>())
    {
        QnMutexLocker lk(&m_impl->mutex);

        //checking queue for reverse task for \a sock
        if (m_impl->removeReverseTask(sock, eventType, TaskType::tRemoving, NULL, 0))
            return;    //ignoring task

        void*& userData = sock->impl()->eventTypeToUserData[eventType];
        NX_ASSERT(userData != NULL);   //socket is not polled. NX_ASSERT?
        std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData = static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data;
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
            m_impl->pollSetModificationQueue.push_back(typename AIOThreadImplType::SocketAddRemoveTask(
                TaskType::tRemoving,
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
            //TODO #ak should receive user handler and call it when removal complete
            m_impl->pollSetModificationQueue.push_back(typename AIOThreadImplType::SocketAddRemoveTask(
                TaskType::tRemoving,
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
    void post( SocketType* const sock, nx::utils::MoveOnlyFunc<void()> functor )
    {
        QnMutexLocker lk(&m_impl->mutex);

        m_impl->pollSetModificationQueue.push_back(
            typename AIOThreadImplType::PostAsyncCallTask(
                sock,
                std::move(functor)));
        //if eventTriggered is lower on stack, socket will be added to pollset before the next poll call
        if (currentThreadSystemId() != systemThreadId())
            m_impl->pollSet.interrupt();
    }

    //!If called in this aio thread, then calls \a functor immediately, otherwise queues \a functor in same way as \a aio::AIOThread::post does
    void dispatch( SocketType* const sock, nx::utils::MoveOnlyFunc<void()> functor )
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
    void cancelPostedCalls( SocketType* const sock, bool waitForRunningHandlerCompletion )
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
                typename AIOThreadImplType::CancelPostedCallsTask(
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
                typename AIOThreadImplType::CancelPostedCallsTask(sock->impl()->socketSequence));
            m_impl->pollSet.interrupt();
        }
    }

    //!Returns number of sockets handled by this object
    size_t socketsHandled() const
    {
        return m_impl->pollSet.size() + m_impl->newReadMonitorTaskCount + m_impl->newWriteMonitorTaskCount;
    }

protected:
    //!Implementation of QThread::run
    virtual void run()
    {
        static const int ERROR_RESET_TIMEOUT = 1000;

        initSystemThreadId();
        NX_LOG(QLatin1String("AIO thread started"), cl_logDEBUG1);

        while (!needToStop())
        {
            //setting processingPostedCalls flag before processPollSetModificationQueue 
            //  to be able to atomically add "cancel posted call" task and check for tasks to complete
            m_impl->processingPostedCalls = 1;

            m_impl->processPollSetModificationQueue(TaskType::tAll);

            //making calls posted with post and dispatch
            m_impl->processPostedCalls();

            m_impl->processingPostedCalls = 0;

            //processing tasks that have been added from within \a processPostedCalls() call
            m_impl->processPollSetModificationQueue(TaskType::tAll);

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
            const int pollTimeout = m_impl->postedCalls.empty() ? millisToTheNextPeriodicEvent : INFINITE_TIMEOUT;
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

private:
    typedef AIOThreadImpl<SocketType, PollSetType> AIOThreadImplType;
    AIOThreadImplType* m_impl;
};



template<class SocketType, class PollSetType>
class AIOThreadImpl
{
public:
    //!TODO #ak better to split this class to multiple ones containing only desired data
    class SocketAddRemoveTask
    {
    public:
        TaskType type;
        SocketType* socket;
        //!Socket number that is still unique after socket has been destroyed
        SocketSequenceType socketSequence;
        aio::EventType eventType;
        AIOEventHandler<SocketType>* eventHandler;
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
            SocketType* const _socket,
            aio::EventType _eventType,
            AIOEventHandler<SocketType>* const _eventHandler,
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
            SocketType* const _socket,
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
        std::shared_ptr<AIOEventHandlingData<SocketType>> data;
        SocketType* socket;
        aio::EventType eventType;

        PeriodicTaskData()
        :
            socket(NULL),
            eventType(aio::etNone)
        {
        }

        PeriodicTaskData(
            const std::shared_ptr<AIOEventHandlingData<SocketType>>& _data,
            SocketType* _socket,
            aio::EventType _eventType)
        :
            data(_data),
            socket(_socket),
            eventType(_eventType)
        {
        }
    };

    //typedef typename socket_to_pollset_static_map::get<SocketType>::value PollSetType;

    //TODO #ak too many mutexes here. Refactoring required

    PollSetType pollSet;
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

    AIOThreadImpl()
    :
        newReadMonitorTaskCount(0),
        newWriteMonitorTaskCount(0),
        processingPostedCalls(0)
    {
        m_monotonicClock.restart();
    }

    //!used as clock for periodic events. Function introduced since implementation can be changed
    qint64 getSystemTimerVal() const
    {
        return m_monotonicClock.elapsed();
    }

    void processPollSetModificationQueue(TaskType taskFilter)
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
                    AIOEventHandlingDataHolder<SocketType>* handlingData = reinterpret_cast<AIOEventHandlingDataHolder<SocketType>*>(userData);
                    //NOTE we are in aio thread currently
                    if (task.timeout > 0)
                    {
                        //adding/updating periodic task
                        if (handlingData->data->timeout > 0)
                            handlingData->data->updatedPeriodicTaskClock = getSystemTimerVal() + task.timeout;  //updating existing task
                        else
                            addPeriodicTask(
                                getSystemTimerVal() + task.timeout,
                                handlingData->data,
                                task.socket,
                                task.eventType);
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

    void addSockToPollset(
        SocketType* socket,
        aio::EventType eventType,
        int timeout,
        AIOEventHandler<SocketType>* eventHandler)
    {
        std::unique_ptr<AIOEventHandlingDataHolder<SocketType>> handlingData(new AIOEventHandlingDataHolder<SocketType>(eventHandler));
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

    void removeSocketFromPollSet(SocketType* sock, aio::EventType eventType)
    {
        //NX_LOG( QString::fromLatin1("removing %1, eventType %2").arg((size_t)sock, 0, 16).arg(eventType), cl_logDEBUG1 );

        void*& userData = sock->impl()->eventTypeToUserData[eventType];
        if (userData)
            delete static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData);
        userData = nullptr;
        if (eventType == aio::etRead || eventType == aio::etWrite)
            pollSet.remove(sock, eventType);
    }

    void removeSocketsFromPollSet()
    {
        processPollSetModificationQueue(TaskType::tRemoving);
    }

    /*!
        This method introduced for optimization: if we fast call watchSocket then removeSocket (socket has not been added to pollset yet),
        than removeSocket can just cancel watchSocket task. And vice versa
        \return \a true if reverse task has been cancelled and socket is already in desired state, no futher processing is needed
    */
    bool removeReverseTask(
        SocketType* const sock,
        aio::EventType eventType,
        TaskType taskType,
        AIOEventHandler<SocketType>* const eventHandler,
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
                static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->timeout = newTimeoutMS;
                static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->markedForRemoval.store(0);

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
    void processSocketEvents(const qint64 curClock)
    {
        for (typename PollSetType::const_iterator
            it = pollSet.begin();
            it != pollSet.end();
            )
        {
            SocketType* const socket = it.socket();
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
            std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData =
                static_cast<AIOEventHandlingDataHolder<SocketType>*>(
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
    bool processPeriodicTasks(const qint64 curClock)
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
            std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData = periodicTaskData.data; //TODO #ak do we really need to copy shared_ptr here?
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

    void processPostedCalls()
    {
        while (!postedCalls.empty())
        {
            auto postHandler = std::move(postedCalls.begin()->postHandler);
            postedCalls.erase(postedCalls.begin());
            postHandler();
        }
    }

    void addPeriodicTask(
        const qint64 taskClock,
        const std::shared_ptr<AIOEventHandlingData<SocketType>>& handlingData,
        SocketType* _socket,
        aio::EventType eventType)
    {
        QnMutexLocker lk(&socketEventProcessingMutex);
        addPeriodicTaskNonSafe(taskClock, handlingData, _socket, eventType);
    }

    void addPeriodicTaskNonSafe(
        const qint64 taskClock,
        const std::shared_ptr<AIOEventHandlingData<SocketType>>& handlingData,
        SocketType* _socket,
        aio::EventType eventType)
    {
        periodicTasksByClock.insert(std::make_pair(
            taskClock,
            PeriodicTaskData(handlingData, _socket, eventType)));
    }

    /** Moves elements to remove to a temporary container and returns it.
        Elements may contain functor which may contain aio objects (sockets) which will be remove
        when removing functor. This may lead to a dead lock if we not release \a lock
     */
    std::vector<SocketAddRemoveTask> cancelPostedCallsInternal(
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

private:
    QElapsedTimer m_monotonicClock;
};

}   //detail


namespace socket_to_pollset_static_map
{
template<class SocketType> struct get {};
template<> struct get<Pollable> { typedef UnifiedPollSet value; };
//template<> struct get<UdtSocket> { typedef UdtPollSet value; };
}

class AbstractAioThread
{
public:
    virtual ~AbstractAioThread() {}
};

template<class SocketType>
class AIOThread
:
    public AbstractAioThread,
    public detail::AIOThread<
        SocketType,
        typename socket_to_pollset_static_map::get<SocketType>::value>
{
    typedef detail::AIOThread<
        SocketType,
        typename socket_to_pollset_static_map::get<SocketType>::value> base_type;

public:
    AIOThread()
    {
    }
};

}   //aio
}   //network
}   //nx

#endif  //AIOTHREAD_H
