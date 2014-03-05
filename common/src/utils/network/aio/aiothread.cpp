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


//TODO/IMPL implement periodic tasks not bound to socket

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
        enum TaskType
        {
            tAdding,
            tRemoving,
            tAll
        };

        QSharedPointer<AbstractSocket> socket;
        PollSet::EventType eventType;
        AIOEventHandler* eventHandler;
        TaskType type;
        int timeout;

        SocketAddRemoveTask(
            const QSharedPointer<AbstractSocket>& _socket,
            PollSet::EventType _eventType,
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
        QSharedPointer<AIOEventHandlingData> data;

        AIOEventHandlingDataHolder( AIOEventHandler* _eventHandler )
        :
            data( new AIOEventHandlingData(_eventHandler) )
        {
        }
    };

    class PeriodicTaskData
    {
    public:
        QSharedPointer<AIOEventHandlingData> data;
        AbstractSocket* socket;

        PeriodicTaskData()
        :
            socket( NULL )
        {
        }

        PeriodicTaskData(
            const QSharedPointer<AIOEventHandlingData>& _data,
            AbstractSocket* _socket )
        :
            data( _data ),
            socket( _socket )
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
                    if( task.eventType == PollSet::etRead )
                        --newReadMonitorTaskCount;
                    else if( task.eventType == PollSet::etWrite )
                        --newWriteMonitorTaskCount;
                    else
                        Q_ASSERT( false );
                }
                else    //task.type == SocketAddRemoveTask::tRemoving
                {
                    void* userData = pollSet.remove( task.socket.data(), task.eventType );
                    if( userData )
                        delete static_cast<AIOEventHandlingDataHolder*>(userData);
                }
                it = pollSetModificationQueue.erase( it );
            }
        }

        bool addSockToPollset(
            QSharedPointer<AbstractSocket> socket,
            PollSet::EventType eventType,
            int timeout,
            AIOEventHandler* eventHandler )
        {
            std::auto_ptr<AIOEventHandlingDataHolder> handlingData( new AIOEventHandlingDataHolder( eventHandler ) );
            if( !pollSet.add( socket.data(), eventType, handlingData.get() ) )
            {
                NX_LOG( lit("Failed to add socket to pollset. %1").arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logWARNING );
                return false;
            }

            if( timeout > 0 )
            {
                //adding periodic task associated with socket
                handlingData->data->timeout = timeout;
                addPeriodicTask( getSystemTimerVal() + timeout, handlingData.get()->data, socket.data() );
            }
            handlingData.release();

            return true;
        }

        void removeSocketsFromPollSet()
        {
            processPollSetModificationQueue( SocketAddRemoveTask::tRemoving );
        }

        bool removeReverseTask(
            const QSharedPointer<AbstractSocket>& sock,
            PollSet::EventType eventType,
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
                        void* userData = pollSet.getUserData( sock.data(), eventType );
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
                QSharedPointer<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(it.userData())->data;
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

        void processPeriodicTasks( const qint64 curClock )
        {
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
                QSharedPointer<AIOEventHandlingData> handlingData = periodicTaskData.data;
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
                        addPeriodicTask( handlingData->updatedPeriodicTaskClock, handlingData, periodicTaskData.socket );
                        handlingData->updatedPeriodicTaskClock = 0;
                        handlingData->beingProcessed.deref();
                        continue;
                    }

                    handlingData->updatedPeriodicTaskClock = 0;
                    //updated task has to be executed now
                }

                if( periodicTaskData.socket )    //periodic event, associated with socket (e.g., socket operation timeout)
                {
                    handlingData->eventHandler->eventTriggered( periodicTaskData.socket, PollSet::etTimedOut );
                    //adding periodic task with same timeout
                    addPeriodicTask( curClock + handlingData->timeout, handlingData, periodicTaskData.socket );
                }
                //else
                //    periodicTaskData.periodicEventHandler->onTimeout( periodicTaskData.taskID );  //for periodic tasks not bound to socket
                handlingData->beingProcessed.deref();
            }
        }

        void addPeriodicTask(
            const qint64 taskClock,
            const QSharedPointer<AIOEventHandlingData>& handlingData,
            AbstractSocket* _socket )
        {
            QMutexLocker lk( &periodicTasksMutex );
            periodicTasksByClock.insert( std::make_pair(
                taskClock,
                PeriodicTaskData(handlingData, _socket) ) );
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
        const QSharedPointer<AbstractSocket>& sock,
        PollSet::EventType eventToWatch,
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
        if( eventToWatch == PollSet::etRead )
            ++m_impl->newReadMonitorTaskCount;
        else if( eventToWatch == PollSet::etWrite )
            ++m_impl->newWriteMonitorTaskCount;
        else
            Q_ASSERT( false );
        if( QThread::currentThread() != this )  //if eventTriggered is lower on stack, socket will be added to pollset before next poll call
            m_impl->pollSet.interrupt();

        return true;
    }

    bool AIOThread::removeFromWatch(
        const QSharedPointer<AbstractSocket>& sock,
        PollSet::EventType eventType,
        bool waitForRunningHandlerCompletion )
    {
        //NOTE m_impl->mutex is locked up the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventType, SocketAddRemoveTask::tRemoving, NULL, 0) )
            return true;    //ignoring task

        void* userData = m_impl->pollSet.getUserData( sock.data(), eventType );
        if( userData == NULL )
            return false;   //assert ???
        QSharedPointer<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(userData)->data;
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
    size_t AIOThread::size( PollSet::EventType eventToWatch ) const
    {
        return m_impl->pollSet.size( eventToWatch ) + 
            (eventToWatch == PollSet::etRead ? m_impl->newReadMonitorTaskCount : m_impl->newWriteMonitorTaskCount);
    }

    //!Returns true, if can monitor one more socket for \a eventToWatch
    bool AIOThread::canAcceptSocket( PollSet::EventType eventToWatch ) const
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
                NX_LOG( QString::fromLatin1( "Poll failed. %1" ).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
                if( errorCode == SystemError::interrupted )
                    continue;
                NX_LOG( lit( "Poll failed. %1" ).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                msleep( ERROR_RESET_TIMEOUT );
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

            //TODO/IMPL #ak recheck that PollSet::remove does not brake PollSet traversal

            m_impl->processPeriodicTasks( curClock );
            m_impl->processSocketEvents( curClock );
        }

        NX_LOG( QLatin1String("AIO thread stopped"), cl_logDEBUG1 );
    }
}
