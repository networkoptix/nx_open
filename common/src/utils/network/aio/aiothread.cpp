/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"

#include <deque>

#include <QAtomicInt>

#include "../../common/log.h"
#include "../../common/systemerror.h"


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

        QSharedPointer<Socket> socket;
        PollSet::EventType eventType;
        AIOEventHandler* eventHandler;
        TaskType type;

        SocketAddRemoveTask(
            const QSharedPointer<Socket>& _socket,
            PollSet::EventType _eventType,
            AIOEventHandler* const _eventHandler,
            TaskType _type )
        :
            socket( _socket ),
            eventType( _eventType ),
            eventHandler( _eventHandler ),
            type( _type )
        {
        }
    };

    class AIOEventHandlingData
    {
    public:
        QAtomicInt beingProcessed;
        QAtomicInt markedForRemoval;
        AIOEventHandler* eventHandler;

        AIOEventHandlingData( AIOEventHandler* _eventHandler )
        :
            eventHandler( _eventHandler )
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

    class AIOThreadImpl
    {
    public:
        PollSet pollSet;
        QMutex* mutex;
        std::deque<SocketAddRemoveTask> pollSetModificationQueue;
        unsigned int newReadMonitorTaskCount;
        unsigned int newWriteMonitorTaskCount;
        mutable QMutex processEventsMutex;

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
                    std::auto_ptr<AIOEventHandlingDataHolder> handlingData( new AIOEventHandlingDataHolder( task.eventHandler ) );
                    if( !pollSet.add( task.socket.data(), task.eventType, handlingData.get() ) )
                        cl_log.log( QString::fromLatin1("Failed to add socket to pollset. %1").arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logWARNING );
                    else
                        handlingData.release();
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

        void removeSocketsFromPollSet()
        {
            processPollSetModificationQueue( SocketAddRemoveTask::tRemoving );
        }

        bool removeReverseTask(
            const QSharedPointer<Socket>& sock,
            PollSet::EventType eventType,
            SocketAddRemoveTask::TaskType taskType,
            AIOEventHandler* const eventHandler )
        {
            Q_UNUSED(eventHandler)

            for( std::deque<SocketAddRemoveTask>::iterator
                it = pollSetModificationQueue.begin();
                it != pollSetModificationQueue.end();
                ++it )
            {
                if( it->socket == sock && it->eventType == eventType && it->type != taskType )
                {
                    //TODO/IMPL if we changing socket handler MUST not remove task
                    if( it->eventType == SocketAddRemoveTask::tRemoving )
                    {
                        //cancelling remove task
                        void* userData = pollSet.getUserData( sock.data(), eventType );
                        Q_ASSERT( userData );
                        static_cast<AIOEventHandlingDataHolder*>(userData)->data->markedForRemoval = 0;
                    }
                    pollSetModificationQueue.erase( it );
                    return true;
                }
            }

            return false;
        }
    };


    AIOThread::AIOThread( QMutex* const mutex )
    :
        m_impl( new AIOThreadImpl() )
    {
        m_impl->mutex = mutex;
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

    //!Monitor socket \a sock for event \a eventToWatch occurence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
    */
    bool AIOThread::watchSocket(
        const QSharedPointer<Socket>& sock,
        PollSet::EventType eventToWatch,
        AIOEventHandler* const eventHandler )
    {
        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventToWatch, SocketAddRemoveTask::tAdding, eventHandler) )
            return true;    //ignoring task

        if( !canAcceptSocket( eventToWatch ) )
            return false;

        m_impl->pollSetModificationQueue.push_back( SocketAddRemoveTask(sock, eventToWatch, eventHandler, SocketAddRemoveTask::tAdding) );
        if( eventToWatch == PollSet::etRead )
            ++m_impl->newReadMonitorTaskCount;
        else if( eventToWatch == PollSet::etWrite )
            ++m_impl->newWriteMonitorTaskCount;
        else
            Q_ASSERT( false );
        m_impl->pollSet.interrupt();
        return true;
    }

    bool AIOThread::removeFromWatch( const QSharedPointer<Socket>& sock, PollSet::EventType eventType )
    {
        //NOTE m_impl->mutex locked up the stack

        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventType, SocketAddRemoveTask::tRemoving, NULL) )
            return true;    //ignoring task

        void* userData = m_impl->pollSet.getUserData( sock.data(), eventType );
        if( userData == NULL )
            return false;   //assert ???
        QSharedPointer<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(userData)->data;
        if( handlingData->markedForRemoval > 0 )
            return false; //socket already marked for removal
        handlingData->markedForRemoval.ref();
        m_impl->pollSetModificationQueue.push_back( SocketAddRemoveTask(sock, eventType, NULL, SocketAddRemoveTask::tRemoving) );
        if( QThread::currentThread() != this )
        {
            m_impl->pollSet.interrupt();

            if( handlingData->beingProcessed > 0 )
            {
                m_impl->mutex->unlock();
                //waiting for event handler to return
                while( handlingData->beingProcessed > 0 )
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
        cl_log.log( QLatin1String("AIO thread started"), cl_logDEBUG1 );

        while( !needToStop() )
        {
            m_impl->processPollSetModificationQueue( SocketAddRemoveTask::tAll );

            int triggeredSocketCount = m_impl->pollSet.poll();
            if( needToStop() )
                break;
            if( triggeredSocketCount == 0 )
                continue;
            if( triggeredSocketCount < 0 )
            {
                cl_log.log( QString::fromLatin1( "Poll failed. %1" ).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                msleep( ERROR_RESET_TIMEOUT );
                continue;
            }

            m_impl->removeSocketsFromPollSet();

            for( PollSet::const_iterator
                it = m_impl->pollSet.begin();
                it != m_impl->pollSet.end();
                ++it )
            {
                //no need to lock mutex, since data is removed in this thread only
                QSharedPointer<AIOEventHandlingData> handlingData = static_cast<AIOEventHandlingDataHolder*>(it.userData())->data;
                QMutexLocker lk( &m_impl->processEventsMutex );
                handlingData->beingProcessed.ref();
                if( handlingData->markedForRemoval > 0 ) //socket has been removed from watch
                {
                    handlingData->beingProcessed.deref();
                    continue;
                }
                handlingData->eventHandler->eventTriggered( it.socket(), it.eventType() );
                handlingData->beingProcessed.deref();
            }
        }

        cl_log.log( QLatin1String("AIO thread stopped"), cl_logDEBUG1 );
    }
}
