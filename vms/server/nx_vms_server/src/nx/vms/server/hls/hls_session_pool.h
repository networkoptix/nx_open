#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <set>

#include <QSharedPointer>
#include <QString>

#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/streaming/media_data_packet.h>
#include <nx/utils/timer_manager.h>

#include "api/model/audit/auth_session.h"
#include "audit/audit_manager_fwd.h"
#include "camera/video_camera.h"
#include "hls_playlist_manager.h"
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::hls {

class AbstractPlaylistManager;

class Session: public ServerModuleAware
{
public:
    /**
     * @param streamQuality If MEDIA_Quality_Auto then both qualities (if available) can be streamed.
     */
    Session(
        QnMediaServerModule* serverModule,
        const QString& id,
        std::chrono::milliseconds targetDuration,
        bool _isLive,
        MediaQuality streamQuality,
        const QnVideoCameraPtr& videoCamera,
        const QnAuthSession& authSession);
    ~Session();

    const QString& id() const;
    std::chrono::milliseconds targetDuration() const;
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

    std::optional<AVCodecID> audioCodecId() const;
    void setAudioCodecId(AVCodecID);

private:
    const QString m_id;
    const std::chrono::milliseconds m_targetDuration;
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
    std::optional<AVCodecID> m_audioCodecId;
};

/**
 * - Owns Session objects.
 * - Removes session if no one uses it during specified timeout. Session cannot be removed while its id is locked.
 * NOTE: It is recommended to lock session id before performing any operations with session.
 * NOTE: Specifing session timeout and working with no id lock can result in undefined behavour.
 * NOTE: Class methods are thread-safe.
 */
class SessionPool:
    public QObject,
    public nx::utils::TimerEventHandler
{
    Q_OBJECT

public:
    /** Locks specified session id for ScopedSessionIDLock lifetime. */
    class ScopedSessionIDLock
    {
    public:
        ScopedSessionIDLock(SessionPool* const pool, const QString& id):
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
        SessionPool* const m_pool;
        const QString m_id;
    };

    virtual ~SessionPool();

    /**
     * Add new session.
     * @param session Object ownership is moved to SessionPool instance.
     * @param keepAliveTimeoutSec Session will be removed,
     * if no one uses it for keepAliveTimeoutSec seconds. 0 is treated as no timeout.
     * @return False, if session with id session->id() already exists. true, otherwise.
     */
    bool add(std::unique_ptr<Session> session, std::chrono::milliseconds keepAliveTimeout);
    /**
     * @return Null, if session not found.
     * NOTE: Re-launches session deletion timer for keepAliveTimeoutSec.
     */
    Session* find(const QString& id) const;
    
    /** Removes session. Deallocates Session object. */
    void remove(const QString& id);

    static QString generateUniqueID();

protected:
    /**
     * If id is already locked, blocks untill it is unlocked.
     */
    void lockSessionID(const QString& id);
    void unlockSessionID(const QString& id);

private:
    class SessionContext
    {
    public:
        std::unique_ptr<Session> session;
        std::chrono::milliseconds keepAliveTimeout = std::chrono::milliseconds::zero();
        quint64 removeTaskId = 0;

        SessionContext() = default;
        SessionContext(
            std::unique_ptr<Session> session,
            std::chrono::milliseconds keepAliveTimeout);
    };

    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::set<QString> m_lockedIDs;
    std::map<QString, SessionContext> m_sessionById;
    std::map<quint64, QString> m_taskToSessionId;

    /** Implementation of TimerEventHandler::onTimer. */
    virtual void onTimer(const quint64& timerId);
    void removeNonSafe(const QString& id);
};

} // namespace nx::vms::server::hls
