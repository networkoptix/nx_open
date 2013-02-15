/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aiothread.h"

#include <deque>

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
                    if( !pollSet.add( task.socket.data(), task.eventType, task.eventHandler ) )
                        cl_log.log( QString::fromLatin1("Failed to add socket to pollset. %1").arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logWARNING );
                    if( task.eventType == PollSet::etRead )
                        --newReadMonitorTaskCount;
                    else if( task.eventType == PollSet::etWrite )
                        --newWriteMonitorTaskCount;
                    else
                        Q_ASSERT( false );
                }
                else    //task.type == SocketAddRemoveTask::tRemoving
                {
                    pollSet.remove( task.socket.data(), task.eventType );
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

    //!Do not monitor \a sock for event \a eventType
    /*!
        Garantees that no \a eventTriggered will be called after return of this method
    */
    void AIOThread::removeFromWatch( const QSharedPointer<Socket>& sock, PollSet::EventType eventType )
    {
        //checking queue for reverse task for \a sock
        if( m_impl->removeReverseTask(sock, eventType, SocketAddRemoveTask::tRemoving, NULL) )
            return;    //ignoring task

        //QMutexLocker lk( &m_impl->processEventsMutex );

        m_impl->pollSetModificationQueue.push_back( SocketAddRemoveTask(sock, eventType, NULL, SocketAddRemoveTask::tRemoving) );
        m_impl->pollSet.interrupt();
        //TODO/IMPL
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

            //QMutexLocker lk( &m_impl->processEventsMutex );

            for( PollSet::const_iterator
                it = m_impl->pollSet.begin();
                it != m_impl->pollSet.end();
                ++it )
            {
                static_cast<AIOEventHandler*>(it.userData())->eventTriggered( it.socket(), it.eventType() );
            }
        }

        cl_log.log( QLatin1String("AIO thread stopped"), cl_logDEBUG1 );
    }
}
