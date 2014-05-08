////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_session_pool.h"

#include <QMutexLocker>


namespace nx_hls
{
    HLSSession::HLSSession(
        const QString& id,
        bool _isLive,
        MediaQuality streamQuality )
    :
        m_id( id ),
        m_live( _isLive ),
        m_streamQuality( streamQuality )
    {
        //verifying m_playlistManagers will not take much memory
        static_assert(
            ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
            "MediaQuality enum suddenly contains too large values: consider changing HLSSession::m_playlistManagers type" );  
        m_playlistManagers.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    }

    const QString& HLSSession::id() const
    {
        return m_id;
    }

    bool HLSSession::isLive() const
    {
        return m_live;
    }

    MediaQuality HLSSession::streamQuality() const
    {
        return m_streamQuality;
    }

    void HLSSession::setPlaylistManager( MediaQuality streamQuality, const QSharedPointer<AbstractPlaylistManager>& value )
    {
        assert( streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low );
        m_playlistManagers[streamQuality] = value;
    }

    const QSharedPointer<AbstractPlaylistManager>& HLSSession::playlistManager( MediaQuality streamQuality ) const
    {
        assert( streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low );
        return m_playlistManagers[streamQuality];
    }


    ////////////////////////////////////////////////////////////
    // HLSSessionPool class
    ////////////////////////////////////////////////////////////

    HLSSessionPool::HLSSessionContext::HLSSessionContext()
    :
        session( NULL ),
        keepAliveTimeoutSec( 0 ),
        removeTaskID( 0 )
    {
    }

    HLSSessionPool::HLSSessionContext::HLSSessionContext(
        HLSSession* const _session,
        int _keepAliveTimeoutSec )
    :
        session( _session ),
        keepAliveTimeoutSec( _keepAliveTimeoutSec ),
        removeTaskID( 0 )
    {
    }


    bool HLSSessionPool::add( HLSSession* session, int keepAliveTimeoutSec )
    {
        QMutexLocker lk( &m_mutex );
        if( !m_sessionByID.insert( std::make_pair(session->id(), HLSSessionContext(session, keepAliveTimeoutSec)) ).second )
            return false;
        Q_ASSERT( (keepAliveTimeoutSec == 0) || (m_lockedIDs.find(session->id()) != m_lockedIDs.end()) );   //session with keep-alive timeout can only be accessed under lock
        return true;
    }

    HLSSession* HLSSessionPool::find( const QString& id ) const
    {
        QMutexLocker lk( &m_mutex );
        std::map<QString, HLSSessionContext>::const_iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() )
            return NULL;
        Q_ASSERT( (it->second.keepAliveTimeoutSec == 0) || (m_lockedIDs.find(id) != m_lockedIDs.end()) );   //session with keep-alive timeout can only be accessed under lock
        return it->second.session;
    }

    void HLSSessionPool::remove( const QString& id )
    {
        QMutexLocker lk( &m_mutex );
        removeNonSafe( id );
    }

    Q_GLOBAL_STATIC( HLSSessionPool, staticInstance )

    HLSSessionPool* HLSSessionPool::instance()
    {
        return staticInstance();
    }

    static QAtomicInt nextSessionID = 1;

    QString HLSSessionPool::generateUniqueID()
    {
        return QString::number(nextSessionID.fetchAndAddOrdered(1));
    }

    void HLSSessionPool::lockSessionID( const QString& id )
    {
        QMutexLocker lk( &m_mutex );
        while( !m_lockedIDs.insert(id).second )
            m_cond.wait( lk.mutex() );

        //removing session remove task (if any)
        std::map<QString, HLSSessionContext>::iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() || it->second.removeTaskID == 0 )
            return;
        m_taskToSessionID.erase( it->second.removeTaskID );
        it->second.removeTaskID = 0;
    }

    static const unsigned int MS_IN_SEC = 1000;

    void HLSSessionPool::unlockSessionID( const QString& id )
    {
        QMutexLocker lk( &m_mutex );
        m_lockedIDs.erase( id );

        //creating session remove task
        std::map<QString, HLSSessionContext>::iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() || it->second.keepAliveTimeoutSec == 0 )
            return;
        it->second.removeTaskID = TimerManager::instance()->addTimer( this, it->second.keepAliveTimeoutSec * MS_IN_SEC );
        m_taskToSessionID.insert( std::make_pair( it->second.removeTaskID, id ) );
    }

    void HLSSessionPool::onTimer( const quint64& timerID )
    {
        QMutexLocker lk( &m_mutex );
        std::map<quint64, QString>::iterator timerIter = m_taskToSessionID.find( timerID );
        if( timerIter == m_taskToSessionID.end() )
            return; //it is possible just after lockSessionID call
        removeNonSafe( timerIter->second );    //removes timerIter also
    }

    void HLSSessionPool::removeNonSafe( const QString& id )
    {
        std::map<QString, HLSSessionContext>::const_iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() )
            return;
        if( it->second.removeTaskID != 0 )
            m_taskToSessionID.erase( it->second.removeTaskID );
        delete it->second.session;
        m_sessionByID.erase( it );
    }
}
