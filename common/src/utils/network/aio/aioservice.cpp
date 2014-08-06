/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <memory>
#include <thread>

#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <qglobal.h>

#include <utils/common/log.h>

#include "aiothread.h"


using namespace std;

namespace aio
{
    static unsigned int threadCountArgValue = 0;

    AIOService::AIOService( unsigned int threadCount )
    {
        if( !threadCount )
            threadCount = threadCountArgValue;

        if( !threadCount )
            threadCount = QThread::idealThreadCount();

        for( unsigned int i = 0; i < threadCount; ++i )
        {
            std::unique_ptr<AIOThread> thread( new AIOThread( &m_mutex ) );
            thread->start();
            if( !thread->isRunning() )
                continue;
            m_threadPool.push_back( thread.release() );
        }

        assert( !m_threadPool.empty() );
        if( m_threadPool.empty() )
        {
            //I wish we used exceptions
            NX_LOG( lit("Could not start a single AIO thread. Terminating..."), cl_logALWAYS );
            std::cerr << "Could not start a single AIO thread. Terminating..." << std::endl;
            exit(1);
        }
    }

    AIOService::~AIOService()
    {
        m_sockets.clear();
        for( std::list<AIOThread*>::iterator
            it = m_threadPool.begin();
            it != m_threadPool.end();
            ++it )
        {
            delete *it;
        }
        m_threadPool.clear();
    }

    //!Returns true, if object has been successfully initialized
    bool AIOService::isInitialized() const
    {
        return !m_threadPool.empty();
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
        QMutexLocker lk( &m_mutex );

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
        map<pair<AbstractSocket*, aio::EventType>, AIOThread*>::iterator it = m_sockets.lower_bound( sockCtx );
        if( it != m_sockets.end() && it->first == sockCtx )
            return true;    //socket already monitored for eventToWatch

        if( (it != m_sockets.end()) && (it->first.first == sockCtx.first) )
        {
            //socket is already monitored for other event. Trying to use same thread
            if( it->second->watchSocket( sock, eventToWatch, eventHandler, sockTimeoutMS ) )
            {
                m_sockets.insert( it, make_pair( sockCtx, it->second ) );
                return true;
            }
            assert( false );    //we MUST use same thread for monitoring all events on single socket
        }

        //searching for a least-used thread, which is ready to accept
        AIOThread* threadToUse = NULL;
        for( std::list<AIOThread*>::const_iterator
            threadIter = m_threadPool.begin();
            threadIter != m_threadPool.end();
            ++threadIter )
        {
            if( !(*threadIter)->canAcceptSocket(sock) )
                continue;
            if( threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled() )
                continue;
            threadToUse = *threadIter;
        }

        if( !threadToUse )
        {
            //creating new thread
            std::unique_ptr<AIOThread> newThread( new AIOThread(&m_mutex) );
            newThread->start();
            if( !newThread->isRunning() )
                return false;
            threadToUse = newThread.get();
            m_threadPool.push_back( newThread.release() );
        }

        if( threadToUse->watchSocket( sock, eventToWatch, eventHandler, sockTimeoutMS ) )
        {
            m_sockets.insert( make_pair( sockCtx, threadToUse ) );
            return true;
        }

        return false;
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

        const pair<AbstractSocket*, aio::EventType>& sockCtx = make_pair( sock, eventType );
        map<pair<AbstractSocket*, aio::EventType>, AIOThread*>::iterator it = m_sockets.find( sockCtx );
        if( it != m_sockets.end() )
        {
            if( it->second->removeFromWatch( sock, eventType, waitForRunningHandlerCompletion ) )
                m_sockets.erase( it );
        }
    }

    bool AIOService::isSocketBeingWatched(AbstractSocket* sock) const
    {
        QMutexLocker lk(&m_mutex);
        const auto& it = m_sockets.lower_bound(std::make_pair(sock, aio::etNone));
        return it != m_sockets.end() && it->first.first == sock;
    }

    Q_GLOBAL_STATIC( AIOService, aioServiceInstance )
    
    AIOService* AIOService::instance( unsigned int threadCount )
    {
        threadCountArgValue = threadCount;
        return aioServiceInstance();
    }
}
