#include "hls_session_pool.h"

#include <core/resource_management/resource_pool.h>

#include "audit/audit_manager.h"
#include "camera/camera_pool.h"
#include "camera/video_camera.h"
#include "core/resource/resource.h"

namespace nx {
namespace mediaserver {
namespace hls {

Session::Session(
    const QString& id,
    unsigned int targetDurationMS,
    bool _isLive,
    MediaQuality streamQuality,
    const QnVideoCameraPtr& videoCamera,
    const QnAuthSession& authSession)
    :
    m_id(id),
    m_targetDurationMS(targetDurationMS),
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
        m_auditHandle = qnAuditManager->notifyPlaybackStarted(
            m_authSession,
            m_cameraId,
            m_live ? DATETIME_NOW : timeUsec);
    }
    if (m_auditHandle)
        qnAuditManager->notifyPlaybackInProgress(m_auditHandle, timeUsec);
}

Session::~Session()
{
    if (m_live)
    {
        QnResourcePtr resource = m_resPool->getResourceById(m_cameraId);
        if (resource)
        {
            //checking resource stream type. Only h.264 is OK for HLS
            QnVideoCameraPtr camera = qnCameraPool->getVideoCamera(resource);
            if (camera)
                camera->notInUse(this);
        }
    }
}

const QString& Session::id() const
{
    return m_id;
}

unsigned int Session::targetDurationMS() const
{
    return m_targetDurationMS;
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
    m_chunksByAlias[std::make_pair(streamQuality, alias)] = std::make_pair(startTimestamp, duration);
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

SessionPool::SessionContext::SessionContext():
    session(NULL),
    keepAliveTimeoutMS(0),
    removeTaskID(0)
{
}

SessionPool::SessionContext::SessionContext(
    Session* const _session,
    unsigned int _keepAliveTimeoutMS)
    :
    session(_session),
    keepAliveTimeoutMS(_keepAliveTimeoutMS),
    removeTaskID(0)
{
}

static SessionPool* SessionPool_instance = nullptr;

SessionPool::SessionPool()
{
    NX_ASSERT(SessionPool_instance == nullptr);
    SessionPool_instance = this;
}

SessionPool::~SessionPool()
{
    while (!m_sessionByID.empty())
    {
        SessionContext sessionCtx;
        {
            QnMutexLocker lk(&m_mutex);
            if (m_sessionByID.empty())
                break;
            sessionCtx = m_sessionByID.begin()->second;
            m_taskToSessionID.erase(sessionCtx.removeTaskID);
            m_sessionByID.erase(m_sessionByID.begin());
        }

        delete sessionCtx.session;
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(sessionCtx.removeTaskID);
    }

    SessionPool_instance = nullptr;
}

bool SessionPool::add(Session* session, unsigned int keepAliveTimeoutMS)
{
    QnMutexLocker lk(&m_mutex);
    if (!m_sessionByID.emplace(session->id(), SessionContext(session, keepAliveTimeoutMS)).second)
        return false;
    // Session with keep-alive timeout can only be accessed under lock.
    NX_ASSERT((keepAliveTimeoutMS == 0) || (m_lockedIDs.find(session->id()) != m_lockedIDs.end()));
    return true;
}

Session* SessionPool::find(const QString& id) const
{
    QnMutexLocker lk(&m_mutex);
    std::map<QString, SessionContext>::const_iterator it = m_sessionByID.find(id);
    if (it == m_sessionByID.end())
        return NULL;
    // Session with keep-alive timeout can only be accessed under lock.
    NX_ASSERT((it->second.keepAliveTimeoutMS == 0) || (m_lockedIDs.find(id) != m_lockedIDs.end()));
    return it->second.session;
}

void SessionPool::remove(const QString& id)
{
    QnMutexLocker lk(&m_mutex);
    removeNonSafe(id);
}

SessionPool* SessionPool::instance()
{
    return SessionPool_instance;
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

    //removing session remove task (if any)
    std::map<QString, SessionContext>::iterator it = m_sessionByID.find(id);
    if (it == m_sessionByID.end() || it->second.removeTaskID == 0)
        return;
    m_taskToSessionID.erase(it->second.removeTaskID);
    it->second.removeTaskID = 0;
}

void SessionPool::unlockSessionID(const QString& id)
{
    QnMutexLocker lk(&m_mutex);
    m_lockedIDs.erase(id);
    m_cond.wakeAll();

    //creating session remove task
    std::map<QString, SessionContext>::iterator it = m_sessionByID.find(id);
    if (it == m_sessionByID.end() || it->second.keepAliveTimeoutMS == 0)
        return;
    it->second.removeTaskID = nx::utils::TimerManager::instance()->addTimer(
        this, std::chrono::milliseconds(it->second.keepAliveTimeoutMS));
    m_taskToSessionID.insert(std::make_pair(it->second.removeTaskID, id));
}

void SessionPool::onTimer(const quint64& timerID)
{
    QnMutexLocker lk(&m_mutex);
    std::map<quint64, QString>::iterator timerIter = m_taskToSessionID.find(timerID);
    if (timerIter == m_taskToSessionID.end())
        return; //it is possible just after lockSessionID call
    removeNonSafe(timerIter->second);    //removes timerIter also
}

void SessionPool::removeNonSafe(const QString& id)
{
    std::map<QString, SessionContext>::const_iterator it = m_sessionByID.find(id);
    if (it == m_sessionByID.end())
        return;
    if (it->second.removeTaskID != 0)
        m_taskToSessionID.erase(it->second.removeTaskID);
    delete it->second.session;
    m_sessionByID.erase(it);
}

} // namespace hls
} // namespace mediaserver
} // namespace nx

