////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SESSION_POOL_H
#define HLS_SESSION_POOL_H

#include <QSharedPointer>
#include <QString>

#include "hls_playlist_manager.h"


namespace nx_hls
{
    class AbstractPlaylistManager;

    class HLSSession
    {
    public:
        HLSSession( const QString& id );

        const QString& id() const;
        void setLive( bool value );
        bool isLive() const;

        void setPlaylistManager( const QSharedPointer<AbstractPlaylistManager>& value );
        AbstractPlaylistManager* playlistManager() const;

    private:
        QString m_id;
        QSharedPointer<AbstractPlaylistManager> m_playlistManager;
    };

    /*!
        \note Class methods are thread-safe
    */
    class HLSSessionPool
    {
    public:
        //!Add
        /*!
            \param session Object ownership is moved to \a HLSSessionPool instance
            \param keepAliveTimeoutSec Session will be removed, if noone uses it for \a keepAliveTimeoutSec seconds
        */
        void add( HLSSession* session, int keepAliveTimeoutSec );
        /*!
            \return NULL, if session not found
            \note Re-launches session deletion timer for \a keepAliveTimeoutSec
        */
        HLSSession* find( const QString& id ) const;

        static HLSSessionPool* instance();
        static QString generateUniqueID();
    };
}

#endif  //HLS_SESSION_POOL_H
