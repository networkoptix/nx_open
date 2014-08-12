/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <memory>
#include <thread>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <qglobal.h>

#include <utils/common/log.h>

#include "aiothread.h"


using namespace std;

namespace aio
{
    typedef AIOThread<PollSet> SystemAIOThread;


    class SocketMonitorContext
    {
    public:
        SystemAIOThread* thread;
        unsigned int monitoredEvents;
    };

    class AIOServiceImpl
    {
    public:
        std::list<SystemAIOThread*> m_systemAioThreadPool;
        std::map<std::pair<AbstractSocket*, aio::EventType>, SystemAIOThread*> m_sockets;
        mutable QMutex m_mutex;
    };


    static unsigned int threadCountArgValue = 0;

    AIOService::AIOService( unsigned int threadCount )
    :
        m_impl( new AIOServiceImpl() )
    {
        if( !threadCount )
            threadCount = threadCountArgValue;

        if( !threadCount )
            threadCount = QThread::idealThreadCount();

        for( unsigned int i = 0; i < threadCount; ++i )
        {
            std::unique_ptr<SystemAIOThread> thread( new SystemAIOThread( &m_impl->m_mutex ) );
            thread->start();
            if( !thread->isRunning() )
                continue;
            m_impl->m_systemAioThreadPool.push_back( thread.release() );
        }

        assert( !m_impl->m_systemAioThreadPool.empty() );
        if( m_impl->m_systemAioThreadPool.empty() )
        {
            //I wish we used exceptions
            NX_LOG( lit("Could not start a single AIO thread. Terminating..."), cl_logALWAYS );
            std::cerr << "Could not start a single AIO thread. Terminating..." << std::endl;
            exit(1);
        }
    }

    AIOService::~AIOService()
    {
        m_impl->m_sockets.clear();
        for( std::list<SystemAIOThread*>::iterator
            it = m_impl->m_systemAioThreadPool.begin();
            it != m_impl->m_systemAioThreadPool.end();
            ++it )
        {
            delete *it;
        }
        m_impl->m_systemAioThreadPool.clear();

        delete m_impl;
        m_impl = nullptr;
    }

    //!Returns true, if object has been successfully initialized
    bool AIOService::isInitialized() const
    {
        return !m_impl->m_systemAioThreadPool.empty();
    }

    //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
    */
    bool AIOService::watchSocket(
        AbstractSocket* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler )
    {
        QMutexLocker lk( &m_impl->m_mutex );
        return watchSocketNonSafe( sock, eventToWatch, eventHandler );
    }

    //!Do not monitor \a sock for event \a eventType
    /*!
        Garantees that no \a eventTriggered will be called after return of this method
    */
    void AIOService::removeFromWatch(
        AbstractSocket* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion )
    {
        QMutexLocker lk( &m_mutex );
        return removeFromWatchNonSafe( sock, eventType, waitForRunningHandlerCompletion );
    }

    bool AIOService::isSocketBeingWatched(AbstractSocket* sock) const
    {
        QMutexLocker lk(&m_impl->m_mutex);
        const auto& it = m_sockets.lower_bound(std::make_pair(sock, aio::etNone));
        return it != m_sockets.end() && it->first.first == sock;
    }

    bool AIOService::watchSocketNonSafe(
        AbstractSocket* const sock,
        aio::EventType eventToWatch,
        AIOEventHandler* const eventHandler )
    {
        unsigned int sockTimeoutMS = 0;
        if( eventToWatch == aio::etRead )
        {
            if( !sock->getRecvTimeout( &sockTimeoutMS ) )
                return false;
        }
        else if( eventToWatch == aio::etWrite )
        {
            if( !sock->getSendTimeout( &sockTimeoutMS ) )
                return false;
        }

        //const int sockTimeoutMS = eventToWatch == aio::etRead
        //    ? sock->getReadTimeOut()
        //    : (eventToWatch == aio::etWrite ? sock->getWriteTimeOut() : 0);

        //checking, if that socket is already monitored
        const pair<AbstractSocket*, aio::EventType>& sockCtx = make_pair( sock, eventToWatch );
        map<pair<AbstractSocket*, aio::EventType>, SystemAIOThread*>::iterator it = m_impl->m_sockets.lower_bound( sockCtx );
        if( it != m_impl->m_sockets.end() && it->first == sockCtx )
            return true;    //socket already monitored for eventToWatch

        if( (it != m_impl->m_sockets.end()) && (it->first.first == sockCtx.first) )
        {
            //socket is already monitored for other event. Trying to use same thread
            if( it->second->watchSocket( sock, eventToWatch, eventHandler, sockTimeoutMS ) )
            {
                m_impl->m_sockets.insert( it, make_pair( sockCtx, it->second ) );
                return true;
            }
            assert( false );    //we MUST use same thread for monitoring all events on single socket
        }

        //searching for a least-used thread, which is ready to accept
        SystemAIOThread* threadToUse = NULL;
        for( std::list<SystemAIOThread*>::const_iterator
            threadIter = m_impl->m_systemAioThreadPool.begin();
            threadIter != m_impl->m_systemAioThreadPool.end();
        ++threadIter )
        {
            if( !(*threadIter)->canAcceptSocket( sock ) )
                continue;
            if( threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled() )
                continue;
            threadToUse = *threadIter;
        }

        if( !threadToUse )
        {
            //creating new thread
            std::unique_ptr<SystemAIOThread> newThread( new SystemAIOThread(&m_impl->m_mutex) );
            newThread->start();
            if( !newThread->isRunning() )
                return false;
            threadToUse = newThread.get();
            m_impl->m_systemAioThreadPool.push_back( newThread.release() );
        }

        if( threadToUse->watchSocket( sock, eventToWatch, eventHandler, sockTimeoutMS ) )
        {
            m_impl->m_sockets.insert( make_pair( sockCtx, threadToUse ) );
            return true;
        }

        return false;
    }

    void AIOService::removeFromWatchNonSafe(
        AbstractSocket* const sock,
        aio::EventType eventType,
        bool waitForRunningHandlerCompletion )
    {
        const pair<AbstractSocket*, aio::EventType>& sockCtx = make_pair( sock, eventType );
        map<pair<AbstractSocket*, aio::EventType>, SystemAIOThread*>::iterator it = m_impl->m_sockets.find( sockCtx );
        if( it != m_impl->m_sockets.end() )
        {
            if( it->second->removeFromWatch( sock, eventType, waitForRunningHandlerCompletion ) )
                m_impl->m_sockets.erase( it );
        }
    }

    bool AIOService::isSocketBeingWatched(AbstractSocket* sock) const
    {
        QMutexLocker lk(&m_impl->m_mutex);
        const auto& it = m_impl->m_sockets.lower_bound(std::make_pair(sock, aio::etNone));
        return it != m_impl->m_sockets.end() && it->first.first == sock;
    }

    Q_GLOBAL_STATIC( AIOService, aioServiceInstance )
    
    AIOService* AIOService::instance( unsigned int threadCount )
    {
        threadCountArgValue = threadCount;
        return aioServiceInstance();
    }
}
