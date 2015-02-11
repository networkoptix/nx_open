////////////////////////////////////////////////////////////
// 7 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SESSION_POOL_H
#define HLS_SESSION_POOL_H

#include <map>
#include <set>

#include <utils/common/mutex.h>
#include <QSharedPointer>
#include <QString>
#include <utils/common/wait_condition.h>

#include <core/datapacket/media_data_packet.h>
#include <utils/common/timermanager.h>

#include "hls_playlist_manager.h"


class QnVideoCamera;

namespace nx_hls
{
    class AbstractPlaylistManager;

    class HLSSession
    {
    public:
        /*!
            \param streamQuality If \a MEDIA_Quality_Auto, than both qualities (if available) can be streamed
        */
        HLSSession(
            const QString& id,
            unsigned int targetDurationMS,
            bool _isLive,
            MediaQuality streamQuality,
            QnVideoCamera* const videoCamera );
        ~HLSSession();

        const QString& id() const;
        unsigned int targetDurationMS() const;
        bool isLive() const;
        MediaQuality streamQuality() const;

        void setPlaylistManager( MediaQuality streamQuality, const AbstractPlaylistManagerPtr& value );
        const AbstractPlaylistManagerPtr& playlistManager( MediaQuality streamQuality ) const;

        void saveChunkAlias( MediaQuality streamQuality, const QString& alias, quint64 startTimestamp, quint64 duration );
        bool getChunkByAlias( MediaQuality streamQuality, const QString& alias, quint64* const startTimestamp, quint64* const duration ) const;

    private:
        const QString m_id;
        const unsigned int m_targetDurationMS;
        const bool m_live;
        const MediaQuality m_streamQuality;
        QnVideoCamera* const m_videoCamera;
        std::vector<AbstractPlaylistManagerPtr> m_playlistManagers;
        //!map<pair<quality, alias>, pair<start timestamp, duration> >
        std::map<std::pair<MediaQuality, QString>, std::pair<quint64, quint64> > m_chunksByAlias;
        mutable QnMutex m_mutex;
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

        HLSSessionPool();
        virtual ~HLSSessionPool();

        //!Add new session
        /*!
            \param session Object ownership is moved to \a HLSSessionPool instance
            \param keepAliveTimeoutSec Session will be removed, if noone uses it for \a keepAliveTimeoutSec seconds. 0 is treated as no timeout
            \return false, if session with id \a session->id() already exists. true, otherwise
        */
        bool add( HLSSession* session, unsigned int keepAliveTimeoutMS );
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
            HLSSession* session;
            unsigned int keepAliveTimeoutMS;
            quint64 removeTaskID;

            HLSSessionContext();
            HLSSessionContext(
                HLSSession* const _session,
                unsigned int _keepAliveTimeoutMS );
        };

        mutable QnMutex m_mutex;
        QnWaitCondition m_cond;
        std::set<QString> m_lockedIDs;
        std::map<QString, HLSSessionContext> m_sessionByID;
        std::map<quint64, QString> m_taskToSessionID;

        //!Implementation of TimerEventHandler::onTimer
        virtual void onTimer( const quint64& timerID );
        void removeNonSafe( const QString& id );
    };
}

#endif  //HLS_SESSION_POOL_H
