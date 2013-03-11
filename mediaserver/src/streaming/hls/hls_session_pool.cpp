////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_session_pool.h"


namespace nx_hls
{
    HLSSession::HLSSession( const QString& id )
    :
        m_id( id )
    {
    }

    const QString& HLSSession::id() const
    {
        return m_id;
    }

    void HLSSession::setPlaylistManager( const QSharedPointer<AbstractPlaylistManager>& value )
    {
        m_playlistManager = value;
    }

    AbstractPlaylistManager* HLSSession::playlistManager() const
    {
        return m_playlistManager.data();
    }


    ////////////////////////////////////////////////////////////
    // HLSSessionPool class
    ////////////////////////////////////////////////////////////

    Q_GLOBAL_STATIC( HLSSessionPool, staticInstance )

    void HLSSessionPool::add( HLSSession* /*session*/, int keepAliveTimeoutSec )
    {
        //TODO/IMPL
    }

    HLSSession* HLSSessionPool::find( const QString& /*id*/ ) const
    {
        //TODO/IMPL
        return false;
    }

    HLSSessionPool* HLSSessionPool::instance()
    {
        return staticInstance();
    }

    static QAtomicInt nextSessionID = 1;

    QString HLSSessionPool::generateUniqueID()
    {
        return QString::number(nextSessionID.fetchAndAddOrdered(1));
    }
}
