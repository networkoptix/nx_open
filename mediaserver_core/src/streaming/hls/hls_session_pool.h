#pragma once 

#include <map>
#include <set>

#include <QSharedPointer>
#include <QString>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/streaming/media_data_packet.h>
#include <nx/utils/timer_manager.h>

#include "api/model/audit/auth_session.h"
#include "audit/audit_manager_fwd.h"
#include "camera/video_camera.h"
#include "hls_playlist_manager.h"

namespace nx_hls {

class AbstractPlaylistManager;

class HLSSession
{
public:
    /**
     * @param streamQuality If MEDIA_Quality_Auto then both qualities (if available) can be streamed.
     */
    HLSSession(
        const QString& id,
        unsigned int targetDurationMS,
        bool _isLive,
        MediaQuality streamQuality,
        const QnVideoCameraPtr& videoCamera,
        const QnAuthSession& authSession);
    ~HLSSession();

    const QString& id() const;
    unsigned int targetDurationMS() const;
    bool isLive() const;
    MediaQuality streamQuality() const;

    void setPlaylistManager(MediaQuality streamQuality, const AbstractPlaylistManagerPtr& value);
    const AbstractPlaylistManagerPtr& playlistManager(MediaQuality streamQuality) const;

    void saveChunkAlias(
        MediaQuality streamQuality,
        const QString& alias,
        quint64 startTimestamp,
        quint64 duration);
    bool getChunkByAlias(
        MediaQuality streamQuality,
        const QString& alias,
        quint64* const startTimestamp,
        quint64* const duration) const;

    void setPlaylistAuthenticationQueryItem(const QPair<QString, QString>& authenticationQueryItem);
    QPair<QString, QString> playlistAuthenticationQueryItem() const;
    void setChunkAuthenticationQueryItem(const QPair<QString, QString>& authenticationQueryItem);
    QPair<QString, QString> chunkAuthenticationQueryItem() const;
    void updateAuditInfo(qint64 timeUsec);

private:
    const QString m_id;
    const unsigned int m_targetDurationMS;
    const bool m_live;
    const MediaQuality m_streamQuality;
    const QnUuid m_cameraId;
    QnResourcePool* m_resPool;

    std::vector<AbstractPlaylistManagerPtr> m_playlistManagers;
    /** map<pair<quality, alias>, pair<start timestamp, duration>>. */
    std::map<std::pair<MediaQuality, QString>, std::pair<quint64, quint64>> m_chunksByAlias;
    QPair<QString, QString> m_playlistAuthenticationQueryItem;
    QPair<QString, QString> m_chunkAuthenticationQueryItem;
    mutable QnMutex m_mutex;
    AuditHandle m_auditHandle;
    QnAuthSession m_authSession;
};

/**
 * - Owns HLSSession objects.
 * - Removes session if no one uses it during specified timeout. Session cannot be removed while its id is locked.
 * NOTE: It is recommended to lock session id before performing any operations with session.
 * NOTE: Specifing session timeout and working with no id lock can result in undefined behavour.
 * NOTE: Class methods are thread-safe.
 */
class HLSSessionPool:
    public nx::utils::TimerEventHandler
{
public:
    /** Locks specified session id for ScopedSessionIDLock lifetime. */
    class ScopedSessionIDLock
    {
    public:
        ScopedSessionIDLock(HLSSessionPool* const pool, const QString& id):
            m_pool(pool),
            m_id(id)
        {
            m_pool->lockSessionID(m_id);
        }

        ~ScopedSessionIDLock()
        {
            m_pool->unlockSessionID(m_id);
        }

    private:
        HLSSessionPool* const m_pool;
        const QString m_id;
    };

    HLSSessionPool();
    virtual ~HLSSessionPool();

    /**
     * Add new session.
     * @param session Object ownership is moved to HLSSessionPool instance.
     * @param keepAliveTimeoutSec Session will be removed,
     * if no one uses it for keepAliveTimeoutSec seconds. 0 is treated as no timeout.
     * @return False, if session with id session->id() already exists. true, otherwise.
     */
    bool add(HLSSession* session, unsigned int keepAliveTimeoutMS);
    /**
     * @return Null, if session not found.
     * NOTE: Re-launches session deletion timer for keepAliveTimeoutSec.
     */
    HLSSession* find(const QString& id) const;
    /** Removes session. Deallocates HLSSession object. */
    void remove(const QString& id);

    static HLSSessionPool* instance();
    static QString generateUniqueID();

protected:
    /**
     * If id is already locked, blocks untill it is unlocked.
     */
    void lockSessionID(const QString& id);
    void unlockSessionID(const QString& id);

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
            unsigned int _keepAliveTimeoutMS);
    };

    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::set<QString> m_lockedIDs;
    std::map<QString, HLSSessionContext> m_sessionByID;
    std::map<quint64, QString> m_taskToSessionID;

    /** Implementation of TimerEventHandler::onTimer. */
    virtual void onTimer(const quint64& timerID);
    void removeNonSafe(const QString& id);
};

} // namespace nx_hls
