#include "hls_session_pool.h"

#include <core/resource_management/resource_pool.h>

#include "audit/audit_manager.h"
#include "camera/camera_pool.h"
#include "camera/video_camera.h"
#include "core/resource/resource.h"
#include <media_server/media_server_module.h>

namespace nx::vms::server::hls {

Session::Session(
    QnMediaServerModule* serverModule,
    const QString& id,
    std::chrono::milliseconds targetDurationMS,
    bool _isLive,
    MediaQuality streamQuality,
    const QnVideoCameraPtr& videoCamera,
    const QnAuthSession& authSession)
    :
    ServerModuleAware(serverModule),
    m_id(id),
    m_targetDuration(targetDurationMS),
    m_live(_isLive),
    m_streamQuality(streamQuality),
    m_cameraId(videoCamera->resource()->getId()),
    m_resPool(videoCamera->resource()->resourcePool()),
    m_authSession(authSession)
{
    // Verifying m_playlistManagers will not take much memory.
    static_assert(
        ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
        "MediaQuality enum suddenly contains too large values: consider changing Session::m_playlistManagers type");
    m_playlistManagers.resize(std::max<>(MEDIA_Quality_High, MEDIA_Quality_Low) + 1);
    if (m_live)
        videoCamera->inUse(this);
}

void Session::updateAuditInfo(qint64 timeUsec)
{
    if (!m_auditHandle)
    {
        m_auditHandle = serverModule()->auditManager()->notifyPlaybackStarted(
            m_authSession,
            m_cameraId,
            m_live ? DATETIME_NOW : timeUsec);
    }

    if (m_auditHandle)
        serverModule()->auditManager()->notifyPlaybackInProgress(m_auditHandle, timeUsec);
}

std::optional<AVCodecID> Session::audioCodecId() const
{
    return m_audioCodecId;
}

void Session::setAudioCodecId(AVCodecID audioCodecId)
{
    m_audioCodecId = audioCodecId;
}

Session::~Session()
{
    if (m_live)
    {
        QnResourcePtr resource = m_resPool->getResourceById(m_cameraId);
        if (resource)
        {
            //checking resource stream type. Only h.264 is OK for HLS
            QnVideoCameraPtr camera = videoCameraPool()->getVideoCamera(resource);
            if (camera)
                camera->notInUse(this);
        }
    }
}

const QString& Session::id() const
{
    return m_id;
}

std::chrono::milliseconds Session::targetDuration() const
{
    return m_targetDuration;
}

bool Session::isLive() const
{
    return m_live;
}

MediaQuality Session::streamQuality() const
{
    return m_streamQuality;
}

void Session::setPlaylistManager(
    MediaQuality streamQuality,
    const AbstractPlaylistManagerPtr& value)
{
    NX_ASSERT(streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low);
    m_playlistManagers[streamQuality] = value;
}

const AbstractPlaylistManagerPtr& Session::playlistManager(MediaQuality streamQuality) const
{
    NX_ASSERT(streamQuality == MEDIA_Quality_High || MEDIA_Quality_Low);
    return m_playlistManagers[streamQuality];
}

void Session::saveChunkAlias(
    MediaQuality streamQuality,
    const QString& alias,
    quint64 startTimestamp,
    quint64 duration)
{
    QnMutexLocker lk(&m_mutex);

    m_chunksByAlias[std::make_pair(streamQuality, alias)] =
        std::make_pair(startTimestamp, duration);
}

bool Session::getChunkByAlias(
    MediaQuality streamQuality,
    const QString& alias,
    quint64* const startTimestamp,
    quint64* const duration) const
{
    QnMutexLocker lk(&m_mutex);

    auto iter = m_chunksByAlias.find(std::make_pair(streamQuality, alias));
    if (iter == m_chunksByAlias.end())
        return false;
    *startTimestamp = iter->second.first;
    *duration = iter->second.second;
    return true;
}

void Session::setPlaylistAuthenticationQueryItem(
    const QPair<QString, QString>& authenticationQueryItem)
{
    m_playlistAuthenticationQueryItem = authenticationQueryItem;
}

QPair<QString, QString> Session::playlistAuthenticationQueryItem() const
{
    return m_playlistAuthenticationQueryItem;
}

void Session::setChunkAuthenticationQueryItem(
    const QPair<QString, QString>& authenticationQueryItem)
{
    m_chunkAuthenticationQueryItem = authenticationQueryItem;
}

QPair<QString, QString> Session::chunkAuthenticationQueryItem() const
{
    return m_chunkAuthenticationQueryItem;
}

//-------------------------------------------------------------------------------------------------
// class SessionPool.

SessionPool::SessionContext::SessionContext(
    std::unique_ptr<Session> session,
    std::chrono::milliseconds keepAliveTimeout)
    :
    session(std::move(session)),
    keepAliveTimeout(keepAliveTimeout)
{
}

SessionPool::~SessionPool()
{
    while (!m_sessionById.empty())
    {
        SessionContext sessionCtx;
        {
            QnMutexLocker lk(&m_mutex);
            if (m_sessionById.empty())
                break;
            sessionCtx = std::move(m_sessionById.begin()->second);
            m_taskToSessionId.erase(sessionCtx.removeTaskId);
            m_sessionById.erase(m_sessionById.begin());
        }

        sessionCtx.session.reset();
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(sessionCtx.removeTaskId);
    }
}

bool SessionPool::add(
    std::unique_ptr<Session> session,
    std::chrono::milliseconds keepAliveTimeout)
{
    QnMutexLocker lk(&m_mutex);

    const auto sessionId = session->id();
    if (!m_sessionById.emplace(
            sessionId,
            SessionContext(std::move(session), keepAliveTimeout)).second)
    {
        return false;
    }

    // Session with keep-alive timeout can only be accessed under lock.
    NX_ASSERT(
        (keepAliveTimeout == std::chrono::milliseconds::zero()) ||
        (m_lockedIDs.find(sessionId) != m_lockedIDs.end()));
    return true;
}

Session* SessionPool::find(const QString& id) const
{
    QnMutexLocker lk(&m_mutex);

    auto it = m_sessionById.find(id);
    if (it == m_sessionById.end())
        return NULL;

    // Session with keep-alive timeout can only be accessed under lock.
    NX_ASSERT(
        (it->second.keepAliveTimeout == std::chrono::milliseconds::zero()) ||
        (m_lockedIDs.find(id) != m_lockedIDs.end()));
    return it->second.session.get();
}

void SessionPool::remove(const QString& id)
{
    QnMutexLocker lk(&m_mutex);
    removeNonSafe(id);
}

static QAtomicInt nextSessionID = 1;

QString SessionPool::generateUniqueID()
{
    return QString::number(nextSessionID.fetchAndAddOrdered(1));
}

void SessionPool::lockSessionID(const QString& id)
{
    QnMutexLocker lk(&m_mutex);

    while (!m_lockedIDs.insert(id).second)
        m_cond.wait(lk.mutex());

    // Removing session remove task (if any).
    auto it = m_sessionById.find(id);
    if (it == m_sessionById.end() || it->second.removeTaskId == 0)
        return;
    m_taskToSessionId.erase(it->second.removeTaskId);
    it->second.removeTaskId = 0;
}

void SessionPool::unlockSessionID(const QString& id)
{
    QnMutexLocker lk(&m_mutex);

    m_lockedIDs.erase(id);
    m_cond.wakeAll();

    //creating session remove task
    auto it = m_sessionById.find(id);
    if (it == m_sessionById.end() || (it->second.keepAliveTimeout == std::chrono::milliseconds::zero()))
        return;

    it->second.removeTaskId = nx::utils::TimerManager::instance()->addTimer(
        this, it->second.keepAliveTimeout);
    m_taskToSessionId.emplace(it->second.removeTaskId, id);
}

void SessionPool::onTimer(const quint64& timerId)
{
    QnMutexLocker lk(&m_mutex);

    auto timerIter = m_taskToSessionId.find(timerId);
    if (timerIter == m_taskToSessionId.end())
        return; //it is possible just after lockSessionID call

    removeNonSafe(timerIter->second);    //removes timerIter also
}

void SessionPool::removeNonSafe(const QString& id)
{
    auto it = m_sessionById.find(id);
    if (it == m_sessionById.end())
        return;

    if (it->second.removeTaskId != 0)
        m_taskToSessionId.erase(it->second.removeTaskId);
    m_sessionById.erase(it);
}

} // namespace nx::vms::server::hls
