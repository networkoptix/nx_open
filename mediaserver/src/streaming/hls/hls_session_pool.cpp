////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_session_pool.h"

#include <QMutexLocker>

#include "camera/video_camera.h"


namespace nx_hls
{
    HLSSession::HLSSession(
        const QString& id,
        unsigned int targetDurationMS,
        bool _isLive,
        MediaQuality streamQuality,
        QnVideoCamera* const videoCamera )
    :
        m_id( id ),
        m_targetDurationMS( targetDurationMS ),
        m_live( _isLive ),
        m_streamQuality( streamQuality ),
        m_videoCamera( videoCamera )
    {
        //verifying m_playlistManagers will not take much memory
        static_assert(
            ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
            "MediaQuality enum suddenly contains too large values: consider changing HLSSession::m_playlistManagers type" );  
        m_playlistManagers.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
        m_videoCamera->inUse( this );
    }

    HLSSession::~HLSSession()
    {
        m_videoCamera->notInUse( this );
    }

    const QString& HLSSession::id() const
    {
        return m_id;
    }

    unsigned int HLSSession::targetDurationMS() const
    {
        return m_targetDurationMS;
    }

    bool HLSSession::isLive() const
    {
        return m_live;
    }

    MediaQuality HLSSession::streamQuality() const
    {
        return m_streamQuality;
    }

    void HLSSession::setPlaylistManager( MediaQuality streamQuality, const AbstractPlaylistManagerPtr& value )
    {
        assert( streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low );
        m_playlistManagers[streamQuality] = value;
    }

    const AbstractPlaylistManagerPtr& HLSSession::playlistManager( MediaQuality streamQuality ) const
    {
        assert( streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low );
        return m_playlistManagers[streamQuality];
    }

    void HLSSession::saveChunkAlias( MediaQuality streamQuality, const QString& alias, quint64 startTimestamp, quint64 duration )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_chunksByAlias[std::make_pair(streamQuality, alias)] = std::make_pair( startTimestamp, duration );
    }

    bool HLSSession::getChunkByAlias( MediaQuality streamQuality, const QString& alias, quint64* const startTimestamp, quint64* const duration ) const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        auto iter = m_chunksByAlias.find( std::make_pair(streamQuality, alias) );
        if( iter == m_chunksByAlias.end() )
            return false;
        *startTimestamp = iter->second.first;
        *duration = iter->second.second;
        return true;
    }


    ////////////////////////////////////////////////////////////
    // HLSSessionPool class
    ////////////////////////////////////////////////////////////

    HLSSessionPool::HLSSessionContext::HLSSessionContext()
    :
        session( NULL ),
        keepAliveTimeoutMS( 0 ),
        removeTaskID( 0 )
    {
    }

    HLSSessionPool::HLSSessionContext::HLSSessionContext(
        HLSSession* const _session,
        unsigned int _keepAliveTimeoutMS )
    :
        session( _session ),
        keepAliveTimeoutMS( _keepAliveTimeoutMS ),
        removeTaskID( 0 )
    {
    }

    static HLSSessionPool* HLSSessionPool_instance = nullptr;

    HLSSessionPool::HLSSessionPool()
    {
        assert( HLSSessionPool_instance == nullptr );
        HLSSessionPool_instance = this;
    }

    HLSSessionPool::~HLSSessionPool()
    {
        while( !m_sessionByID.empty() )
        {
            HLSSessionContext sessionCtx;
            {
                SCOPED_MUTEX_LOCK( lk,  &m_mutex );
                if( m_sessionByID.empty() )
                    break;
                sessionCtx = m_sessionByID.begin()->second;
                m_taskToSessionID.erase( sessionCtx.removeTaskID );
                m_sessionByID.erase( m_sessionByID.begin() );
            }

            delete sessionCtx.session;
            TimerManager::instance()->joinAndDeleteTimer( sessionCtx.removeTaskID );
        }

        HLSSessionPool_instance = nullptr;
    }

    bool HLSSessionPool::add( HLSSession* session, unsigned int keepAliveTimeoutMS )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        if( !m_sessionByID.insert( std::make_pair(session->id(), HLSSessionContext(session, keepAliveTimeoutMS)) ).second )
            return false;
        Q_ASSERT( (keepAliveTimeoutMS == 0) || (m_lockedIDs.find(session->id()) != m_lockedIDs.end()) );   //session with keep-alive timeout can only be accessed under lock
        return true;
    }

    HLSSession* HLSSessionPool::find( const QString& id ) const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        std::map<QString, HLSSessionContext>::const_iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() )
            return NULL;
        Q_ASSERT( (it->second.keepAliveTimeoutMS == 0) || (m_lockedIDs.find(id) != m_lockedIDs.end()) );   //session with keep-alive timeout can only be accessed under lock
        return it->second.session;
    }

    void HLSSessionPool::remove( const QString& id )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        removeNonSafe( id );
    }

    HLSSessionPool* HLSSessionPool::instance()
    {
        return HLSSessionPool_instance;
    }

    static QAtomicInt nextSessionID = 1;

    QString HLSSessionPool::generateUniqueID()
    {
        return QString::number(nextSessionID.fetchAndAddOrdered(1));
    }

    void HLSSessionPool::lockSessionID( const QString& id )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        while( !m_lockedIDs.insert(id).second )
            m_cond.wait( lk.mutex() );

        //removing session remove task (if any)
        std::map<QString, HLSSessionContext>::iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() || it->second.removeTaskID == 0 )
            return;
        m_taskToSessionID.erase( it->second.removeTaskID );
        it->second.removeTaskID = 0;
    }

    void HLSSessionPool::unlockSessionID( const QString& id )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_lockedIDs.erase( id );

        //creating session remove task
        std::map<QString, HLSSessionContext>::iterator it = m_sessionByID.find( id );
        if( it == m_sessionByID.end() || it->second.keepAliveTimeoutMS == 0 )
            return;
        it->second.removeTaskID = TimerManager::instance()->addTimer( this, it->second.keepAliveTimeoutMS );
        m_taskToSessionID.insert( std::make_pair( it->second.removeTaskID, id ) );
    }

    void HLSSessionPool::onTimer( const quint64& timerID )
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
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
