/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"

#include <atomic>
#include <deque>
#include <memory>

#include <QtCore/QDateTime>

#include "../system_socket.h"
#include "../../common/log.h"
#include "../../common/systemerror.h"

//TODO #ak memory order semantic used with std::atomic
//TODO #ak move task queues to socket for optimization
//TODO #ak should add some sequence which will be incremented when socket is started polling on any event
    //this is required to distinguish tasks. E.g., socket is polled, than polling stopped asynchronously, 
    //before async stop completion socket is polled again. In this case remove task should not remove tasks 
    //from new polling "session"? Something needs to be done about it


//!used as clock for periodic events. Function introduced since implementation can be changed
static qint64 getSystemTimerVal()
{
    return QDateTime::currentMSecsSinceEpoch();
}

namespace aio
{
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

        AIOEventHandlingData( AIOEventHandler<SocketType>* _eventHandler )
        :
            beingProcessed( 0 ),
            markedForRemoval( 0 ),
            eventHandler( _eventHandler ),
            timeout( 0 ),
            updatedPeriodicTaskClock( 0 )
        {
        }
    };

    template<class SocketType>
    class AIOEventHandlingDataHolder
    {
    public:
        //!Why the fuck do we need shared_ptr inside object instanciated on heap?
        std::shared_ptr<AIOEventHandlingData<SocketType>> data;

        AIOEventHandlingDataHolder( AIOEventHandler<SocketType>* _eventHandler )
        :
            data( new AIOEventHandlingData<SocketType>( _eventHandler ) )
        {
        }
    };


    namespace socket_to_pollset_static_map
    {
        template<class SocketType> struct get {};
        template<> struct get<Pollable> { typedef PollSet value; };
    }


    template<class SocketType>
    class AIOThreadImpl
    {
    public:
        class SocketAddRemoveTask
        {
        public:
            TaskType type;
            SocketType* socket;
            aio::EventType eventType;
            AIOEventHandler<SocketType>* eventHandler;
            //!0 means no timeout
            unsigned int timeout;
            std::atomic<int>* taskCompletionEvent;
            std::function<void()> postHandler;

            /*!
                \param taskCompletionEvent if not NULL, set to 1 after processing task
            */
            SocketAddRemoveTask(
                TaskType _type,
                SocketType* const _socket,
                aio::EventType _eventType,
                AIOEventHandler<SocketType>* const _eventHandler,
                unsigned int _timeout = 0,
                std::atomic<int>* const _taskCompletionEvent = nullptr )
            :
                type( _type ),
                socket( _socket ),
                eventType( _eventType ),
                eventHandler( _eventHandler ),
                timeout( _timeout ),
                taskCompletionEvent( _taskCompletionEvent )
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
                std::function<void()>&& _postHandler )
            :
                SocketAddRemoveTask(
                    TaskType::tCallFunc,
                    _socket,
                    aio::etNone,
                    nullptr,
                    0,
                    nullptr )
            {
                this->postHandler = std::move(_postHandler);
            }
        };

        class CancelPostedCallsTask
        :
            public SocketAddRemoveTask
        {
        public:
            CancelPostedCallsTask(
                SocketType* const _socket,
                std::atomic<int>* const _taskCompletionEvent = nullptr )
            :
                SocketAddRemoveTask(
                    TaskType::tCancelPostedCalls,
                    _socket,
                    aio::etNone,
                    nullptr,
                    0,
                    _taskCompletionEvent )
            {
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
                socket( NULL ),
                eventType( aio::etNone )
            {
            }

            PeriodicTaskData(
                const std::shared_ptr<AIOEventHandlingData<SocketType>>& _data,
                SocketType* _socket,
                aio::EventType _eventType )
            :
                data( _data ),
                socket( _socket ),
                eventType( _eventType )
            {
            }
        };

        typedef typename socket_to_pollset_static_map::get<SocketType>::value PollSetType;

        //TODO #ak too many mutexes here. Refactoring required

        PollSetType pollSet;
        QMutex* const aioServiceMutex;
        std::deque<SocketAddRemoveTask> pollSetModificationQueue;
        unsigned int newReadMonitorTaskCount;
        unsigned int newWriteMonitorTaskCount;
        mutable QMutex mutex;
        std::multimap<qint64, PeriodicTaskData> periodicTasksByClock;
        std::deque<SocketAddRemoveTask> postedCalls;

        AIOThreadImpl( QMutex* const _aioServiceMutex )
        :
            aioServiceMutex( _aioServiceMutex ),
            newReadMonitorTaskCount( 0 ),
            newWriteMonitorTaskCount( 0 )
        {
        }

        void processPollSetModificationQueue( TaskType taskFilter )
        {
            if( pollSetModificationQueue.empty() )
                return;

            QMutexLocker lk( aioServiceMutex );

            for( typename std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                 )
            {
                const SocketAddRemoveTask& task = *it;
                if( (taskFilter != TaskType::tAll) && (task.type != taskFilter) )
                {
                    ++it;
                    continue;
                }

                switch( task.type )
                {
                    case TaskType::tAdding:
                    { 
                        if( task.eventType == aio::etRead )
                            --newReadMonitorTaskCount;
                        else if( task.eventType == aio::etWrite )
                            --newWriteMonitorTaskCount;
                        addSockToPollset( task.socket, task.eventType, task.timeout, task.eventHandler );
                        break;
                    }

                    case TaskType::tChangingTimeout:
                    {
                        void* userData = task.socket->impl()->eventTypeToUserData[task.eventType];
                        AIOEventHandlingDataHolder<SocketType>* handlingData = reinterpret_cast<AIOEventHandlingDataHolder<SocketType>*>( userData );
                        //NOTE we are in aio thread currently
                        if( task.timeout > 0 )
                        {
                            //adding/updating periodic task
                            if( handlingData->data->timeout > 0 )
                                handlingData->data->updatedPeriodicTaskClock = getSystemTimerVal() + task.timeout;  //updating existing task
                            else
                                addPeriodicTask(
                                    getSystemTimerVal() + task.timeout,
                                    handlingData->data,
                                    task.socket,
                                    task.eventType );
                        }
                        else if( handlingData->data->timeout > 0 )  //&& timeout == 0
                            handlingData->data->updatedPeriodicTaskClock = -1;  //cancelling existing periodic task (there must be one)
                        handlingData->data->timeout = task.timeout;
                        break;
                    }

                    case TaskType::tRemoving:
                        removeSocketFromPollSet( task.socket, task.eventType );
                        break;

                    case TaskType::tCallFunc:
                    {
                        assert( task.postHandler );
                        postedCalls.push_back( std::move(task) );
                        break;
                    }

                    case TaskType::tCancelPostedCalls:
                    {
                        cancelPostedCallsInternal( task.socket );
                        break;
                    }

                    default:
                        assert( false );
                }
                if( task.taskCompletionEvent )
                    task.taskCompletionEvent->store( 1, std::memory_order_relaxed );
                it = pollSetModificationQueue.erase( it );
            }
        }

        bool addSockToPollset(
            SocketType* socket,
            aio::EventType eventType,
            int timeout,
            AIOEventHandler<SocketType>* eventHandler )
        {
            std::unique_ptr<AIOEventHandlingDataHolder<SocketType>> handlingData( new AIOEventHandlingDataHolder<SocketType>( eventHandler ) );
            if( eventType != aio::etTimedOut )
                if( !pollSet.add( socket, eventType, handlingData.get() ) )
                {
                    const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("Failed to add socket to pollset. %1").arg(SystemError::toString(errorCode)), cl_logWARNING);
                    return false;
                }
            socket->impl()->eventTypeToUserData[eventType] = handlingData.get();

            if( timeout > 0 )
            {
                //adding periodic task associated with socket
                handlingData->data->timeout = timeout;
                addPeriodicTask(
                    getSystemTimerVal() + timeout,
                    handlingData.get()->data,
                    socket,
                    eventType );
            }
            handlingData.release();

            return true;
        }

        void removeSocketFromPollSet( SocketType* sock, aio::EventType eventType )
        {
            void*& userData = sock->impl()->eventTypeToUserData[eventType];
            if( userData )
                delete static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData);
            userData = nullptr;
            if( eventType == aio::etRead || eventType == aio::etWrite )
                pollSet.remove( sock, eventType );
        }

        void removeSocketsFromPollSet()
        {
            processPollSetModificationQueue( TaskType::tRemoving );
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
            unsigned int newTimeoutMS )
        {
            Q_UNUSED(eventHandler)

            for( typename std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                ++it )
            {
                if( !(it->socket == sock && it->eventType == eventType && taskType != it->type) )
                    continue;

                //removing reverse task (if any)
                if( taskType == TaskType::tAdding && it->type == TaskType::tRemoving )
                {
                    //cancelling removing socket
                    if( eventHandler != it->eventHandler )
                        continue;   //event handler changed, cannot ignore task
                    //cancelling remove task
                    void* userData = sock->impl()->eventTypeToUserData[eventType];
                    Q_ASSERT( userData );
                    static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->timeout = newTimeoutMS;
                    static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->markedForRemoval.store( 0 );

                    pollSetModificationQueue.erase( it );
                    return true;
                }
                else if( taskType == TaskType::tRemoving && (it->type == TaskType::tAdding || it->type == TaskType::tChangingTimeout) )
                {
                    //if( it->type == TaskType::tChangingTimeout ) cancelling scheduled tasks, since socket removal from poll is requested
                    const TaskType foundTaskType = it->type;

                    //cancelling adding socket
                    it = pollSetModificationQueue.erase( it );
                    //removing futher tChangingTimeout tasks
                    for( ;
                        it != pollSetModificationQueue.end();
                        ++it )
                    {
                        if( it->socket == sock && it->eventType == eventType )
                        {
                            assert( it->type == TaskType::tChangingTimeout );
                            it = pollSetModificationQueue.erase( it );
                        }
                    }

                    if( foundTaskType == TaskType::tAdding )
                        return true;
                    else
                        return false;   //foundTaskEventType == TaskType::tChangingTimeout
                }

                continue;
            }

            return false;
        }

        //!Processes events from \a pollSet
        void processSocketEvents( const qint64 curClock )
        {
            for( typename PollSetType::const_iterator
                it = pollSet.begin();
                it != pollSet.end();
                 )
            {
                SocketType* const socket = it.socket();
                const aio::EventType sockEventType = it.eventType();
                const aio::EventType handlerToInvokeType =
                    socket->impl()->eventTypeToUserData[aio::etRead]
                        ? aio::etRead
                        : aio::etWrite;

                //NOTE in case of error pollSet reports etError once, 
                    //but two handlers may be registered for socket.
                    //So, we must notify both

                //TODO #ak notify second handler (if any) and if it not removed by first handler

                //no need to lock aioServiceMutex, since data is removed in this thread only
                std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData = 
                    static_cast<AIOEventHandlingDataHolder<SocketType>*>(
                        socket->impl()->eventTypeToUserData[handlerToInvokeType])->data;

                QMutexLocker lk( &mutex );
                ++handlingData->beingProcessed;
                //TODO #ak possibly some atomic fence is required here
                if( handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0 ) //socket has been removed from watch
                {
                    --handlingData->beingProcessed;
                    ++it;
                    continue;
                }
                ++it;
                //eventTriggered is allowed to call removeFromWatch which can remove socket from pollset
                handlingData->eventHandler->eventTriggered( socket, sockEventType );
                //updating socket's periodic task (it's garanteed that there is periodic task for socket)
                if( handlingData->timeout > 0 )
                    handlingData->updatedPeriodicTaskClock = curClock + handlingData->timeout;
                --handlingData->beingProcessed;
            }
        }

        /*!
            \return \a true, if at least one task has been processed
        */
        bool processPeriodicTasks( const qint64 curClock )
        {
            int tasksProcessedCount = 0;

            for( ;; )
            {
                QMutexLocker lk( &mutex );

                PeriodicTaskData periodicTaskData;
                {
                    //taking task from queue
                    typename std::multimap<qint64, PeriodicTaskData>::iterator it = periodicTasksByClock.begin();
                    if( it == periodicTasksByClock.end() || it->first > curClock )
                        break;
                    periodicTaskData = it->second;
                    periodicTasksByClock.erase( it );
                }

                //no need to lock aioServiceMutex, since data is removed in this thread only
                std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData = periodicTaskData.data; //TODO #ak do we really need to copy shared_ptr here?
                ++handlingData->beingProcessed;
                //TODO #ak atomic fence is required here (to avoid reordering)
                //TODO #ak add some auto pointer for handlingData->beingProcessed
                if( handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0 ) //task has been removed from watch
                {
                    --handlingData->beingProcessed;
                    continue;
                }

                if( handlingData->updatedPeriodicTaskClock > 0 )
                {
                    //not processing task, but updating...
                    if( handlingData->updatedPeriodicTaskClock > curClock )
                    {
                        //adding new task with updated clock
                        addPeriodicTaskNonSafe(
                            handlingData->updatedPeriodicTaskClock,
                            handlingData,
                            periodicTaskData.socket,
                            periodicTaskData.eventType );
                        handlingData->updatedPeriodicTaskClock = 0;
                        --handlingData->beingProcessed;
                        continue;
                    }

                    handlingData->updatedPeriodicTaskClock = 0;
                    //updated task has to be executed now
                }
                else if( handlingData->updatedPeriodicTaskClock == -1 )
                {
                    //cancelling periodic task
                    handlingData->updatedPeriodicTaskClock = 0;
                    --handlingData->beingProcessed;
                    continue;
                }

                if( periodicTaskData.socket )    //periodic event, associated with socket (e.g., socket operation timeout)
                {
                    //NOTE socket is allowed to be removed in eventTriggered
                    handlingData->eventHandler->eventTriggered(
                        periodicTaskData.socket,
                        static_cast<aio::EventType>(periodicTaskData.eventType | aio::etTimedOut) );
                    //adding periodic task with same timeout
                    addPeriodicTaskNonSafe(
                        curClock + handlingData->timeout,
                        handlingData,
                        periodicTaskData.socket,
                        periodicTaskData.eventType );
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
            while( !postedCalls.empty() )
            {
                auto postHandler = std::move( postedCalls.begin()->postHandler );
                postedCalls.erase( postedCalls.begin() );
                postHandler();
            }
        }

        void addPeriodicTask(
            const qint64 taskClock,
            const std::shared_ptr<AIOEventHandlingData<SocketType>>& handlingData,
            SocketType* _socket,
            aio::EventType eventType )
        {
            QMutexLocker lk( &mutex );
            addPeriodicTaskNonSafe( taskClock, handlingData, _socket, eventType );
        }

        void addPeriodicTaskNonSafe(
            const qint64 taskClock,
            const std::shared_ptr<AIOEventHandlingData<SocketType>>& handlingData,
            SocketType* _socket,
            aio::EventType eventType )
        {
            periodicTasksByClock.insert( std::make_pair(
                taskClock,
                PeriodicTaskData( handlingData, _socket, eventType ) ) );
        }

        void cancelPostedCallsInternal( SocketType* const sock )
        {
            for( typename std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                 )
            {
                if( it->socket == sock && it->type == TaskType::tCallFunc )
                    it = pollSetModificationQueue.erase( it );
                else
                    ++it;
            }

            //TODO #ak copy-paste. Refactor it!

            for( typename std::deque<SocketAddRemoveTask>::iterator
                it = postedCalls.begin();
                it != postedCalls.end();
                 )
            {
                if( it->socket == sock )
                    it = postedCalls.erase( it );
                else
                    ++it;
            }
        }
    };


    template<class SocketType>
    AIOThread<SocketType>::AIOThread( QMutex* const aioServiceMutex ) //TODO: #ak give up using single aioServiceMutex for all aio threads
    :
        m_impl( new AIOThreadImplType( aioServiceMutex ) )
    {
        setObjectName( lit("AIOThread") );
    }

    template<class SocketType>
    AIOThread<SocketType>::~AIOThread()
    {
        pleaseStop();
        wait();

        delete m_impl;
        m_impl = NULL;
    }

    template<class SocketType>
    void AIOThread<SocketType>::pleaseStop()
    {
        QnLongRunnable::pleaseStop();
        m_impl->pollSet.interrupt();
    }

    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
    */
    template<class SocketType>
    bool AIOThread<SocketType>::watchSocket(
        SocketType* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<SocketType>* const eventHandler,
        unsigned int timeoutMs )
    {
        //NOTE m_impl->aioServiceMutex is locked up the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask( sock, eventToWatch, TaskType::tAdding, eventHandler, timeoutMs ) )
            return true;    //ignoring task

        m_impl->pollSetModificationQueue.push_back( typename AIOThreadImplType::SocketAddRemoveTask(
            TaskType::tAdding,
            sock,
            eventToWatch,
            eventHandler,
            timeoutMs ) );
        if( eventToWatch == aio::etRead )
            ++m_impl->newReadMonitorTaskCount;
        else if( eventToWatch == aio::etWrite )
            ++m_impl->newWriteMonitorTaskCount;
        if( currentThreadSystemId() != systemThreadId() )  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
            m_impl->pollSet.interrupt();

        return true;
    }

    template<class SocketType>
    bool AIOThread<SocketType>::changeSocketTimeout(
        SocketType* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler<SocketType>* const eventHandler,
        unsigned int timeoutMs )
    {
        //TODO #ak looks like copy-paste of previous method. Remove copy-paste nahuy!!!

        //NOTE m_impl->aioServiceMutex is locked up the stack

        //this task does not cancel any other task. TODO #ak maybe it should cancel another timeout change?
        ////checking queue for reverse task for \a sock
        //if( m_impl->removeReverseTask( sock, eventToWatch, TaskType::tAdding, eventHandler, timeoutMs ) )
        //    return true;    //ignoring task

        //if socket is marked for removal, not adding task
        void* userData = sock->impl()->eventTypeToUserData[eventToWatch];
        assert( userData != nullptr );  //socket is not polled, but someone wants to change timeout
        if( static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data->markedForRemoval.load(std::memory_order_relaxed) > 0 )
            return true;   //socket marked for removal, ignoring timeout change (like, cancelling it right now)

        m_impl->pollSetModificationQueue.push_back( typename AIOThreadImplType::SocketAddRemoveTask(
            TaskType::tChangingTimeout,
            sock,
            eventToWatch,
            eventHandler,
            timeoutMs ) );
        if( currentThreadSystemId() != systemThreadId() )  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
            m_impl->pollSet.interrupt();

        return true;
    }

    template<class SocketType>
    bool AIOThread<SocketType>::removeFromWatch(
        SocketType* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion )
    {
        //NOTE m_impl->aioServiceMutex is locked down the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask( sock, eventType, TaskType::tRemoving, NULL, 0 ) )
            return true;    //ignoring task

        void*& userData = sock->impl()->eventTypeToUserData[eventType];
        if( userData == NULL )
            return false;   //socket is not polled. assert?
        std::shared_ptr<AIOEventHandlingData<SocketType>> handlingData = static_cast<AIOEventHandlingDataHolder<SocketType>*>(userData)->data;
        if( handlingData->markedForRemoval.load(std::memory_order_relaxed) > 0 )
            return false;   //socket already marked for removal
        ++handlingData->markedForRemoval;

        const bool inAIOThread = currentThreadSystemId() == systemThreadId();

        //inAIOThread is false in case async operation cancellation. In most cases, inAIOThread is true
        if( inAIOThread )
        {
            //removing socket from pollset does not invalidate iterators (iterating pollset may be higher the stack)
            m_impl->removeSocketFromPollSet( sock, eventType );
            userData = nullptr;
        }
        else if( waitForRunningHandlerCompletion )
        {
            std::atomic<int> taskCompletedCondition( 0 );
            //we MUST remove socket from pollset before returning from here
            m_impl->pollSetModificationQueue.push_back( typename AIOThreadImplType::SocketAddRemoveTask(
                TaskType::tRemoving,
                sock,
                eventType,
                nullptr,
                0,
                &taskCompletedCondition ) );

            m_impl->pollSet.interrupt();

            //we can be sure that socket will be removed before next poll

            m_impl->aioServiceMutex->unlock();

            //TODO #ak maybe we just have to wait for remove completion, but not for running handler completion?
                //I.e., is it possible that handler was launched (or still running) after removal task completion?

            //waiting for event handler completion (if it running)
            while( handlingData->beingProcessed.load() > 0 )
            {
                m_impl->mutex.lock();
                m_impl->mutex.unlock();
            }

            //waiting for socket to be removed from pollset
            while( taskCompletedCondition.load( std::memory_order_relaxed ) == 0 )
                msleep( 0 );    //yield. TODO #ak Better replace it with conditional_variable
            //TODO #ak if remove task completed, doesn't it mean handler is not running and never be launched?

            m_impl->aioServiceMutex->lock();
        }
        else
        {
            //TODO #ak should receive user handler and call it when removal complete
            m_impl->pollSetModificationQueue.push_back( typename AIOThreadImplType::SocketAddRemoveTask(
                TaskType::tRemoving,
                sock,
                eventType,
                nullptr,
                0 ) );
            m_impl->pollSet.interrupt();
        }

        return true;
    }

    template<class SocketType>
    bool AIOThread<SocketType>::post( SocketType* const sock, std::function<void()>&& functor )
    {
        m_impl->pollSetModificationQueue.push_back(
            typename AIOThreadImplType::PostAsyncCallTask(
                sock,
                std::move(functor) ) );
        //if eventTriggered is lower on stack, socket will be added to pollset before the next poll call
        if( currentThreadSystemId() != systemThreadId() )
            m_impl->pollSet.interrupt();
        return true;
    }

    template<class SocketType>
    bool AIOThread<SocketType>::dispatch( SocketType* const sock, std::function<void()>&& functor )
    {
        if( currentThreadSystemId() == systemThreadId() )  //if called from this aio thread
        {
            m_impl->aioServiceMutex->unlock();
            functor();
            m_impl->aioServiceMutex->lock();
            return true;
        }
        //otherwise posting functor
        return post( sock, std::move(functor) );
    }

    template<class SocketType>
    void AIOThread<SocketType>::cancelPostedCalls( SocketType* const sock, bool waitForRunningHandlerCompletion )
    {
        const bool inAIOThread = currentThreadSystemId() == systemThreadId();
        if( inAIOThread )
        {
            //removing postedCall tasks and posted calls
            m_impl->cancelPostedCallsInternal( sock );
        }
        else if( waitForRunningHandlerCompletion )
        {
            //posting cancellation task
            std::atomic<int> taskCompletedCondition( 0 );
            //we MUST remove socket from pollset before returning from here
            m_impl->pollSetModificationQueue.push_back(
                typename AIOThreadImplType::CancelPostedCallsTask( sock, &taskCompletedCondition ) );
            m_impl->pollSet.interrupt();

            //we can be sure that socket will be removed before next poll

            m_impl->aioServiceMutex->unlock();

            //waiting for socket to be removed from pollset
            while( taskCompletedCondition.load( std::memory_order_relaxed ) == 0 )
                msleep( 0 );    //yield. TODO #ak Better replace it with conditional_variable

            m_impl->aioServiceMutex->lock();
        }
        else
        {
            m_impl->pollSetModificationQueue.push_back(
                typename AIOThreadImplType::CancelPostedCallsTask( sock ) );
            m_impl->pollSet.interrupt();
        }
    }

    //!Returns number of sockets monitored for \a eventToWatch event
    template<class SocketType>
    size_t AIOThread<SocketType>::socketsHandled() const
    {
        return m_impl->pollSet.size() + m_impl->newReadMonitorTaskCount + m_impl->newWriteMonitorTaskCount;
    }

    static const int ERROR_RESET_TIMEOUT = 1000;

    template<class SocketType>
    void AIOThread<SocketType>::run()
    {
        initSystemThreadId();
        NX_LOG( QLatin1String("AIO thread started"), cl_logDEBUG1 );

        while( !needToStop() )
        {
            m_impl->processPollSetModificationQueue( TaskType::tAll );

            //making calls posted with post and dispatch
            m_impl->processPostedCalls();

            //processing tasks that have been added from within \a processPostedCalls() call
            m_impl->processPollSetModificationQueue( TaskType::tAll );

            qint64 curClock = getSystemTimerVal();
            //taking clock of the next periodic task
            qint64 nextPeriodicEventClock = 0;
            {
                QMutexLocker lk( &m_impl->mutex );
                nextPeriodicEventClock = m_impl->periodicTasksByClock.empty() ? 0 : m_impl->periodicTasksByClock.cbegin()->first;
            }

            //calculating delay to the next periodic task
            const int millisToTheNextPeriodicEvent = nextPeriodicEventClock == 0
                ? aio::INFINITE_TIMEOUT    //no periodic task
                : (nextPeriodicEventClock < curClock ? 0 : nextPeriodicEventClock - curClock);

            //if there are posted calls, just checking sockets state in non-blocking mode
            const int pollTimeout = m_impl->postedCalls.empty() ? millisToTheNextPeriodicEvent : 0;
            const int triggeredSocketCount = m_impl->pollSet.poll( pollTimeout );

            if( needToStop() )
                break;
            if( triggeredSocketCount < 0 )
            {
                const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                if( errorCode == SystemError::interrupted )
                    continue;
                NX_LOG(lit("AIOThread. poll failed. %1").arg(SystemError::toString(errorCode)), cl_logDEBUG1);
                msleep(ERROR_RESET_TIMEOUT);
                continue;
            }
            curClock = getSystemTimerVal(); 
            if( triggeredSocketCount == 0 )
            {
                if( nextPeriodicEventClock == 0 ||      //no periodic event
                    nextPeriodicEventClock > curClock ) //not a time for periodic event
                {
                    continue;   //poll has been interrupted
                }
            }

            m_impl->removeSocketsFromPollSet();

            if( m_impl->processPeriodicTasks( curClock ) )
                continue;   //periodic task handler is allowed to delete socket what can cause undefined behavour while iterating pollset
            if( triggeredSocketCount > 0 )
                m_impl->processSocketEvents( curClock );
        }

        NX_LOG( QLatin1String("AIO thread stopped"), cl_logDEBUG1 );
    }

    template class AIOThread<Pollable>;
}
