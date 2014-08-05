/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"

#include <deque>
#include <memory>

#include <QtCore/QAtomicInt>
#include <QtCore/QDateTime>

#include "../../common/log.h"
#include "../../common/systemerror.h"


//!used as clock for periodic events. Function introduced since implementation can be changed
static qint64 getSystemTimerVal()
{
    return QDateTime::currentMSecsSinceEpoch();
}

namespace aio
{
    class SocketAddRemoveTask
    {
    public:
        // TODO: #Elric #enum
        enum TaskType
        {
            tAdding,
            tRemoving,
            tAll
        };

        AbstractSocket* socket;
        aio::EventType eventType;
        AIOEventHandler* eventHandler;
        TaskType type;
        int timeout;

        SocketAddRemoveTask(
            AbstractSocket* const _socket,
            aio::EventType _eventType,
            AIOEventHandler* const _eventHandler,
            TaskType _type,
            int _timeout )
        :
            socket( _socket ),
            eventType( _eventType ),
            eventHandler( _eventHandler ),
            type( _type ),
            timeout( _timeout )
        {
        }
    };

    //!Used as userdata in PollSet. One \a AIOEventHandlingData object corresponds to pair (\a socket, \a eventType)
    class AIOEventHandlingData
    {
    public:
        QAtomicInt beingProcessed;
        QAtomicInt markedForRemoval;
        AIOEventHandler* eventHandler;
        int timeout;
        qint64 updatedPeriodicTaskClock;

        AIOEventHandlingData( AIOEventHandler* _eventHandler )
        :
            eventHandler( _eventHandler ),
            timeout( 0 ),
            updatedPeriodicTaskClock( 0 )
        {
        }
    };

    class AIOEventHandlingDataHolder
    {
    public:
        std::shared_ptr<AIOEventHandlingData> data;

        AIOEventHandlingDataHolder( AIOEventHandler* _eventHandler )
        :
            data( new AIOEventHandlingData(_eventHandler) )
        {
        }
    };

    class PeriodicTaskData
    {
    public:
        std::shared_ptr<AIOEventHandlingData> data;
        AbstractSocket* socket;
        aio::EventType eventType;

        PeriodicTaskData()
        :
            socket( NULL ),
            eventType( aio::etNone )
        {
        }

        PeriodicTaskData(
            const std::shared_ptr<AIOEventHandlingData>& _data,
            AbstractSocket* _socket,
            aio::EventType _eventType )
        :
            data( _data ),
            socket( _socket ),
            eventType( _eventType )
        {
        }
    };

    class AIOThreadImpl
    {
    public:
        PollSet pollSet;
        QMutex* mutex;
        std::deque<SocketAddRemoveTask> pollSetModificationQueue;
        unsigned int newReadMonitorTaskCount;
        unsigned int newWriteMonitorTaskCount;
        mutable QMutex processEventsMutex;

        //!map<event clock (millis), periodic task data>. todo: #use some thread-safe container on atomic operations instead of map and mutex
        std::multimap<qint64, PeriodicTaskData> periodicTasksByClock;
        QMutex periodicTasksMutex;

        AIOThreadImpl()
        :
            mutex( NULL ),
            newReadMonitorTaskCount( 0 ),
            newWriteMonitorTaskCount( 0 )
        {
        }

        void processPollSetModificationQueue( SocketAddRemoveTask::TaskType taskFilter )
        {
            if( pollSetModificationQueue.empty() )
                return;

            QMutexLocker lk( mutex );

            for( std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                 )
            {
                const SocketAddRemoveTask& task = *it;
                if( (taskFilter != SocketAddRemoveTask::tAll) && (task.type != taskFilter) )
                {
                    ++it;
                    continue;
                }

                if( task.type == SocketAddRemoveTask::tAdding )
                { 
                    addSockToPollset( task.socket, task.eventType, task.timeout, task.eventHandler );
                    if( task.eventType == aio::etRead )
                        --newReadMonitorTaskCount;
                    else if( task.eventType == aio::etWrite )
                        --newWriteMonitorTaskCount;
                    else
                        Q_ASSERT( false );
                }
                else    //task.type == SocketAddRemoveTask::tRemoving
                {
                    void* userData = pollSet.remove( task.socket, task.eventType );
                    if( userData )
                        delete static_cast<AIOEventHandlingDataHolder*>(userData);
                }
                it = pollSetModificationQueue.erase( it );
            }
        }

        bool addSockToPollset(
            AbstractSocket* socket,
            aio::EventType eventType,
            int timeout,
            AIOEventHandler* eventHandler )
        {
            std::auto_ptr<AIOEventHandlingDataHolder> handlingData( new AIOEventHandlingDataHolder( eventHandler ) );
            if( !pollSet.add( socket, eventType, handlingData.get() ) )
            {
                const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                NX_LOG(lit("Failed to add socket to pollset. %1").arg(SystemError::toString(errorCode)), cl_logWARNING);
                return false;
            }

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

        void removeSocketsFromPollSet()
        {
            processPollSetModificationQueue( SocketAddRemoveTask::tRemoving );
        }

        bool removeReverseTask(
            AbstractSocket* const sock,
            aio::EventType eventType,
            SocketAddRemoveTask::TaskType taskType,
            AIOEventHandler* const eventHandler,
            int /*newTimeoutMS*/ )
        {
            Q_UNUSED(eventHandler)

            for( std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                ++it )
            {
                if( it->socket == sock && it->eventType == eventType && it->type != taskType )
                {
                    //TODO/IMPL if we changing socket timeout MUST NOT remove task
                    if( it->type == SocketAddRemoveTask::tRemoving )     
                    {
                        if( eventHandler != it->eventHandler )
                            continue;   //event handler changed, cannot ignore task
                        //cancelling remove task
                        void* userData = pollSet.getUserData( sock, eventType );
                        Q_ASSERT( userData );
                        static_cast<AIOEventHandlingDataHolder*>(userData)->data->markedForRemoval.store(0);
                    }
                    pollSetModificationQueue.erase( it );
                    return true;
                }
            }

            return false;
        }

        //!Processes events from \a pollSet
        void processSocketEvents( const qint64 curClock )
        {
            for( PollSet::const_iterator
                it = pollSet.begin();
                it != pollSet.end();
                ++it )
            {
                //no need to lock mutex, since data is removed in this thread only
                std::shared_ptr<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(it.userData())->data;
                QMutexLocker lk( &processEventsMutex );
                handlingData->beingProcessed.ref();
                if( handlingData->markedForRemoval.load() > 0 ) //socket has been removed from watch
                {
                    handlingData->beingProcessed.deref();
                    continue;
                }
                handlingData->eventHandler->eventTriggered( it.socket(), it.eventType() );
                if( handlingData->timeout > 0 )
                    handlingData->updatedPeriodicTaskClock = curClock + handlingData->timeout;      //updating socket's periodic task (it's garanteed that there is periodic task for socket)
                handlingData->beingProcessed.deref();
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
                PeriodicTaskData periodicTaskData;
                {
                    //taking task from queue
                    QMutexLocker lk( &periodicTasksMutex );
                    std::multimap<qint64, PeriodicTaskData>::iterator it = periodicTasksByClock.begin();
                    if( it == periodicTasksByClock.end() || it->first > curClock )
                        break;
                    periodicTaskData = it->second;
                    periodicTasksByClock.erase( it );
                }

                //no need to lock mutex, since data is removed in this thread only
                std::shared_ptr<AIOEventHandlingData> handlingData = periodicTaskData.data;
                QMutexLocker lk( &processEventsMutex );
                handlingData->beingProcessed.ref();
                if( handlingData->markedForRemoval.load() > 0 ) //task has been removed from watch
                {
                    handlingData->beingProcessed.deref();
                    continue;
                }

                if( handlingData->updatedPeriodicTaskClock > 0 )
                {
                    //not processing task, but updating...
                    if( handlingData->updatedPeriodicTaskClock > curClock )
                    {
                        //adding new task with updated clock
                        addPeriodicTask(
                            handlingData->updatedPeriodicTaskClock,
                            handlingData,
                            periodicTaskData.socket,
                            periodicTaskData.eventType );
                        handlingData->updatedPeriodicTaskClock = 0;
                        handlingData->beingProcessed.deref();
                        continue;
                    }

                    handlingData->updatedPeriodicTaskClock = 0;
                    //updated task has to be executed now
                }

                if( periodicTaskData.socket )    //periodic event, associated with socket (e.g., socket operation timeout)
                {
                    //TODO #ak socket is allowed to be removed in eventTriggered
                    handlingData->eventHandler->eventTriggered(
                        periodicTaskData.socket,
                        static_cast<aio::EventType>(periodicTaskData.eventType | aio::etTimedOut) );
                    //adding periodic task with same timeout
                    addPeriodicTask(
                        curClock + handlingData->timeout,
                        handlingData,
                        periodicTaskData.socket,
                        periodicTaskData.eventType );
                    ++tasksProcessedCount;
                }
                //else
                //    periodicTaskData.periodicEventHandler->onTimeout( periodicTaskData.taskID );  //for periodic tasks not bound to socket
                handlingData->beingProcessed.deref();
            }

            return tasksProcessedCount > 0;
        }

        void addPeriodicTask(
            const qint64 taskClock,
            const std::shared_ptr<AIOEventHandlingData>& handlingData,
            AbstractSocket* _socket,
            aio::EventType eventType )
        {
            QMutexLocker lk( &periodicTasksMutex );
            periodicTasksByClock.insert( std::make_pair(
                taskClock,
                PeriodicTaskData(handlingData, _socket, eventType) ) );
        }
    };


    AIOThread::AIOThread( QMutex* const mutex ) //TODO: #ak give up using single mutex for all aio threads
    :
        m_impl( new AIOThreadImpl() )
    {
        m_impl->mutex = mutex;

        setObjectName( lit("AIOThread") );
    }

    AIOThread::~AIOThread()
    {
        pleaseStop();
        wait();

        delete m_impl;
        m_impl = NULL;
    }

    void AIOThread::pleaseStop()
    {
        QnLongRunnable::pleaseStop();
        m_impl->pollSet.interrupt();
    }

    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
    */
    bool AIOThread::watchSocket(
        AbstractSocket* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler,
        int timeoutMs )
    {
        //NOTE m_impl->mutex is locked up the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventToWatch, SocketAddRemoveTask::tAdding, eventHandler, timeoutMs) )
            return true;    //ignoring task

        if( !canAcceptSocket( eventToWatch ) )
            return false;

        //if( QThread::currentThread() == this )
        //{
        //    //adding socket to pollset right away
        //    return m_impl->addSockToPollset( sock, eventToWatch, timeoutMs, eventHandler );
        //}

        m_impl->pollSetModificationQueue.push_back( SocketAddRemoveTask(sock, eventToWatch, eventHandler, SocketAddRemoveTask::tAdding, timeoutMs) );
        if( eventToWatch == aio::etRead )
            ++m_impl->newReadMonitorTaskCount;
        else if( eventToWatch == aio::etWrite )
            ++m_impl->newWriteMonitorTaskCount;
        else
            Q_ASSERT( false );
        if( QThread::currentThread() != this )  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
            m_impl->pollSet.interrupt();

        return true;
    }

    bool AIOThread::removeFromWatch(
        AbstractSocket* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion )
    {
        //NOTE m_impl->mutex is locked up the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventType, SocketAddRemoveTask::tRemoving, NULL, 0) )
            return true;    //ignoring task

        void* userData = m_impl->pollSet.getUserData( sock, eventType );
        if( userData == NULL )
            return false;   //assert ???
        std::shared_ptr<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(userData)->data;
        if( handlingData->markedForRemoval.load() > 0 )
            return false; //socket already marked for removal
        handlingData->markedForRemoval.ref();
        m_impl->pollSetModificationQueue.push_back( SocketAddRemoveTask(sock, eventType, NULL, SocketAddRemoveTask::tRemoving, 0) );

        if( waitForRunningHandlerCompletion && (QThread::currentThread() != this) )
        {
            m_impl->pollSet.interrupt();

            if( handlingData->beingProcessed.load() > 0 )
            {
                m_impl->mutex->unlock();
                //waiting for event handler to return
                while( handlingData->beingProcessed.load() > 0 )
                {
                    m_impl->processEventsMutex.lock();
                    m_impl->processEventsMutex.unlock();
                }
                m_impl->mutex->lock();
            }
        }

        return true;
    }

    //!Returns number of sockets monitored for \a eventToWatch event
    size_t AIOThread::size( aio::EventType eventToWatch ) const
    {
        return m_impl->pollSet.size( eventToWatch ) + 
            (eventToWatch == aio::etRead ? m_impl->newReadMonitorTaskCount : m_impl->newWriteMonitorTaskCount);
    }

    //!Returns true, if can monitor one more socket for \a eventToWatch
    bool AIOThread::canAcceptSocket( aio::EventType eventToWatch ) const
    {
        return size( eventToWatch ) < m_impl->pollSet.maxPollSetSize();
    }

    static const int ERROR_RESET_TIMEOUT = 1000;

    void AIOThread::run()
    {
        initSystemThreadId();
        NX_LOG( QLatin1String("AIO thread started"), cl_logDEBUG1 );

        while( !needToStop() )
        {
            m_impl->processPollSetModificationQueue( SocketAddRemoveTask::tAll );

            qint64 curClock = getSystemTimerVal();
            //taking clock of the next periodic task
            qint64 nextPeriodicEventClock = 0;
            {
                QMutexLocker lk( &m_impl->periodicTasksMutex );
                nextPeriodicEventClock = m_impl->periodicTasksByClock.empty() ? 0 : m_impl->periodicTasksByClock.begin()->first;
            }

            //calculating delay to the next periodic task
            const int millisToTheNextPeriodicEvent = nextPeriodicEventClock == 0
                ? PollSet::INFINITE_TIMEOUT    //no periodic task
                : (nextPeriodicEventClock < curClock ? 0 : nextPeriodicEventClock - curClock);

            int triggeredSocketCount = m_impl->pollSet.poll(millisToTheNextPeriodicEvent);
            if( needToStop() )
                break;
            if( triggeredSocketCount < 0 )
            {
                const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                if( errorCode == SystemError::interrupted )
                    continue;
                NX_LOG(QString::fromLatin1("Poll failed. %1").arg(SystemError::toString(errorCode)), cl_logDEBUG1);
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

            //TODO #ak recheck that PollSet::remove does not brake PollSet traversal

            if( m_impl->processPeriodicTasks( curClock ) )
                continue;   //periodic task handler is allowed to delete socket what will cause undefined behavour while iterating pollset
            if( triggeredSocketCount > 0 )
                m_impl->processSocketEvents( curClock );
        }

        NX_LOG( QLatin1String("AIO thread stopped"), cl_logDEBUG1 );
    }
}
