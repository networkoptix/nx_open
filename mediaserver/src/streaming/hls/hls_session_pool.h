////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SESSION_POOL_H
#define HLS_SESSION_POOL_H

#include <map>
#include <set>

#include <QMutex>
#include <QSharedPointer>
#include <QString>
#include <QWaitCondition>

#include <utils/common/timermanager.h>

#include "hls_playlist_manager.h"


namespace nx_hls
{
    class AbstractPlaylistManager;

    class HLSSession
    {
    public:
        HLSSession( const QString& id, bool _isLive );

        const QString& id() const;
        bool isLive() const;

        void setPlaylistManager( const QSharedPointer<AbstractPlaylistManager>& value );
        AbstractPlaylistManager* playlistManager() const;

    private:
        QString m_id;
        bool m_live;
        QSharedPointer<AbstractPlaylistManager> m_playlistManager;
    };

    /*!
        - owns \a HLSSession objects
        - removes session if noone uses it during specified timeout. Session cannot be removed while its id is locked

        \note It is recommended to lock session id before performing any operations with session
        \note Specifing session timeout and working with no id lock can result in undefined behavour
        \note Class methods are thread-safe
    */
    class HLSSessionPool
    :
        public TimerEventHandler
    {
    public:
        //!Locks specified session id for \a ScopedSessionIDLock life time
        class ScopedSessionIDLock
        {
        public:
            ScopedSessionIDLock( HLSSessionPool* const pool, const QString& id )
            :
                m_pool( pool ),
                m_id( id )
            {
                m_pool->lockSessionID( m_id );
            }

            ~ScopedSessionIDLock()
            {
                m_pool->unlockSessionID( m_id );
            }

        private:
            HLSSessionPool* const m_pool;
            const QString m_id;
        };

        //!Add new session
        /*!
            \param session Object ownership is moved to \a HLSSessionPool instance
            \param keepAliveTimeoutSec Session will be removed, if noone uses it for \a keepAliveTimeoutSec seconds. 0 is treated as no timeout
            \return false, if session with id \a session->id() already exists. true, otherwise
        */
        bool add( HLSSession* session, int keepAliveTimeoutSec );
        /*!
            \return NULL, if session not found
            \note Re-launches session deletion timer for \a keepAliveTimeoutSec
        */
        HLSSession* find( const QString& id ) const;
        //!Removes session. Deallocates \a HLSSession object
        void remove( const QString& id );

        static HLSSessionPool* instance();
        static QString generateUniqueID();

    protected:
        /*!
            If \a id already locked, blocks till it unlocked
        */
        void lockSessionID( const QString& id );
        void unlockSessionID( const QString& id );

    private:
        class HLSSessionContext
        {
        public:
            HLSSession* const session;
            const int keepAliveTimeoutSec;
            quint64 removeTaskID;

            HLSSessionContext();
            HLSSessionContext(
                HLSSession* const _session,
                int _keepAliveTimeoutSec );
        };

        mutable QMutex m_mutex;
        QWaitCondition m_cond;
        std::set<QString> m_lockedIDs;
        std::map<QString, HLSSessionContext> m_sessionByID;
        std::map<quint64, QString> m_taskToSessionID;

        //!Implementation of TimerEventHandler::onTimer
        virtual void onTimer( const quint64& timerID );
        void removeNonSafe( const QString& id );
    };
}

#endif  //HLS_SESSION_POOL_H
