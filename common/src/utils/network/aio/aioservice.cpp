/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <memory>

#include <QMutexLocker>

#include "aiothread.h"


using namespace std;

namespace aio
{
    AIOService::AIOService( unsigned int threadCount )
    {
        if( !threadCount )
        {
            //TODO/IMPL calculating optimal thread count
            threadCount = 1;
        }

        for( unsigned int i = 0; i < threadCount; ++i )
        {
            std::auto_ptr<AIOThread> thread( new AIOThread( &m_mutex ) );
            thread->start();
            if( !thread->isRunning() )
            {
                //killing every fucking thing we just created
                for( std::list<AIOThread*>::iterator
                    it = m_threadPool.begin();
                    it != m_threadPool.end();
                    ++it )
                {
                    delete *it;
                }
                m_threadPool.clear();
                return;
            }
            m_threadPool.push_back( thread.release() );
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

    //!Monitor socket \a sock for event \a eventToWatch occurence and trigger \a eventHandler on event
    /*!
        \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
    */
    bool AIOService::watchSocket(
        const QSharedPointer<Socket>& sock,
        PollSet::EventType eventToWatch,
        AIOEventHandler* const eventHandler )
    {
        QMutexLocker lk( &m_mutex );

        //checking, if that socket is already monitored
        const pair<Socket*, PollSet::EventType>& sockCtx = make_pair( sock.data(), eventToWatch );
        map<pair<Socket*, PollSet::EventType>, AIOThread*>::const_iterator it = m_sockets.lower_bound( sockCtx );
        if( it != m_sockets.end() && it->first == sockCtx )
            return true;    //socket already monitored for eventToWatch

        if( (it != m_sockets.end()) && (it->first.first == sockCtx.first) )
        {
            //socket is already monitored for other event. Trying to use same thread
            if( it->second->watchSocket( sock, eventToWatch, eventHandler ) )
            {
                m_sockets.insert( it, make_pair( sockCtx, it->second ) );
                return true;
            }
        }

        //searching for to least-used thread, which is ready to accept
        AIOThread* threadToUse = NULL;
        for( std::list<AIOThread*>::const_iterator
            threadIter = m_threadPool.begin();
            threadIter != m_threadPool.end();
            ++threadIter )
        {
            if( !(*threadIter)->canAcceptSocket( eventToWatch ) )
                continue;
            if( threadToUse && threadToUse->size(eventToWatch) < (*threadIter)->size(eventToWatch) )
                continue;
            threadToUse = *threadIter;
        }

        if( !threadToUse )
        {
            //creating new thread
            //TODO:#ak auto_ptr is deprecated. Use unique_ptr instead
            std::auto_ptr<AIOThread> newThread( new AIOThread(&m_mutex) );
            newThread->start();
            if( !newThread->isRunning() )
                return false;
            threadToUse = newThread.get();
            m_threadPool.push_back( newThread.release() );
        }

        if( threadToUse->watchSocket( sock, eventToWatch, eventHandler ) )
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
    void AIOService::removeFromWatch( const QSharedPointer<Socket>& sock, PollSet::EventType eventType )
    {
        QMutexLocker lk( &m_mutex );

        const pair<Socket*, PollSet::EventType>& sockCtx = make_pair( sock.data(), eventType );
        map<pair<Socket*, PollSet::EventType>, AIOThread*>::iterator it = m_sockets.find( sockCtx );
        if( it != m_sockets.end() )
        {
            if( it->second->removeFromWatch( sock, eventType ) )
                m_sockets.erase( it );
        }
    }

    Q_GLOBAL_STATIC( AIOService, aioServiceInstance )
    
    AIOService* AIOService::instance( unsigned int /*threadCount*/ )
    {
        return aioServiceInstance();
    }
}
