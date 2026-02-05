// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_session_pool.h"

#include "webrtc_session.h"

#include <nx/vms/common/system_settings.h>

namespace nx::webrtc {

SessionPool::SessionPool(
    nx::vms::common::SystemContext* context,
    nx::utils::TimerManager* timerManager,
    QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy)
    :
    nx::vms::common::SystemContextAware(context),
    m_timerManager(timerManager),
    m_iceManager(context, this),
    m_threadPool(std::make_unique<QThreadPool>()),
    m_transcodePolicy(transcodePolicy)
{
    static const std::chrono::milliseconds kMinTimeout(100);
    using namespace std::chrono;
    m_keepAliveTimeout = globalSettings()->mediaSendTimeout();
    const milliseconds timeout = std::max(
        kMinTimeout,
        milliseconds(m_keepAliveTimeout.count() / 4));
    using namespace std::placeholders;
    m_timerId = m_timerManager->addNonStopTimer(
        std::bind(&SessionPool::onTimer, this, _1),
        timeout, timeout);
}

/*virtual*/ SessionPool::~SessionPool()
{
    stop();
}

void SessionPool::stop()
{
    m_timerManager->joinAndDeleteTimer(m_timerId);
    m_iceManager.stop();

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sessionById.clear();
}

IceManager& SessionPool::iceManager()
{
  return m_iceManager;
}

QThreadPool* SessionPool::threadPool()
{
    return m_threadPool.get();
}

std::string SessionPool::emitUfrag()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    std::string localUfrag;
    do
    {
        localUfrag = Session::generateLocalUfrag();
    }
    while (m_sessionById.count(localUfrag) > 0);

    m_sessionById[localUfrag] = std::nullopt;
    return localUfrag;
}

bool SessionPool::takeUfrag(const std::string& ufrag)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    if (m_sessionById.count(ufrag) > 0)
        return false;
    m_sessionById[ufrag] = std::nullopt;
    return true;
}

std::shared_ptr<Session> SessionPool::createInternal(
    std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
    Purpose purpose,
    const nx::network::SocketAddress& address,
    const std::string& localUfrag,
    const SessionConfig& config)
{
    auto iceCandidates = m_iceManager.allocateIce(localUfrag, config); //< Should be unguarded.

    auto session = createSessionInternal(
        std::move(cameraProvider), purpose, address, localUfrag, iceCandidates, config);

    // Note that async ices should be created after session creation.
    m_iceManager.allocateIceAsync(localUfrag, config); //< Should be unguarded.

    return session;
}

std::shared_ptr<Session> SessionPool::createSessionInternal(
    std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
    Purpose purpose,
    const nx::network::SocketAddress& address,
    const std::string& localUfrag,
    const std::vector<IceCandidate>& iceCandidates,
    const SessionConfig& config)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    const auto sessionIter = m_sessionById.find(localUfrag);
    if (!NX_ASSERT(sessionIter != m_sessionById.end() && !sessionIter->second,
        "Session ufrag should be taken before by takeUfrag() of emitUfrag()"))
    {
        return nullptr;
    }

    std::shared_ptr<Session> session = std::make_shared<Session>(
        systemContext(),
        this,
        std::move(cameraProvider),
        localUfrag,
        address,
        iceCandidates,
        purpose);

    m_sessionById[session->id()] = SessionContext{
        .session = session,
        .config = config};

    NX_DEBUG(this, "Started session %1", localUfrag);

    return session;
}

std::shared_ptr<Session> SessionPool::create(
    std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
    Purpose purpose,
    const nx::network::SocketAddress& address,
    const std::string& localUfrag,
    const SessionConfig& config)
{
    if (!takeUfrag(localUfrag))
        return nullptr;

    return createInternal(
        std::move(cameraProvider), purpose, address, localUfrag, config);
}

std::shared_ptr<Session> SessionPool::create(
    std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
    Purpose purpose,
    const nx::network::SocketAddress& address,
    const SessionConfig& config)
{
    const auto localUfrag = emitUfrag();
    return createInternal(
        std::move(cameraProvider), purpose, address, localUfrag, config);
}

std::optional<SessionDescription> SessionPool::describe(const std::string& id)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    auto session = m_sessionById.find(id);
    if (session == m_sessionById.end() || !session->second || session->second->isLocked())
        return std::nullopt;

    return session->second->session->describe();
}

std::shared_ptr<Session> SessionPool::lock(const std::string& id)
{
    // TODO: Maybe it is better to cleanup now unused Ices here...
    NX_MUTEX_LOCKER lk(&m_mutex);
    auto session = m_sessionById.find(id);
    if (session == m_sessionById.end() || !session->second || session->second->isLocked())
        return nullptr;

    session->second->lock();
    NX_DEBUG(this, "Locked session %1", id);
    return session->second->session;
}

void SessionPool::unlock(const std::string& id)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    auto session = m_sessionById.find(id);
    NX_CRITICAL(session != m_sessionById.end());
    NX_CRITICAL(session->second);
    if (!session->second->isLocked())
        return; //< unlock() called twice


    session->second->unlock();
    NX_DEBUG(this, "Unlocked session %1", id);
}

/*virtual*/ void SessionPool::onTimer(const quint64& /*timerId*/)
{
    std::vector<SessionPtr> toRemove;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        for (auto it = m_sessionById.begin(); it != m_sessionById.end();)
        {
            auto& ctx = it->second;
            if (ctx && ctx->hasExpired(m_keepAliveTimeout))
            {
                NX_DEBUG(this, "Remove session on timer: %1", ctx->session);
                toRemove.emplace_back(std::move(ctx->session));
                it = m_sessionById.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    for (const auto& session: toRemove)
        m_iceManager.freeIce(session->id()); //< Should be unguarded.
}

void SessionPool::drop(const std::string& id)
{
    SessionPtr session;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto sessionIter = m_sessionById.find(id);
        if (sessionIter == m_sessionById.end())
            return; //< Already dropped by some way.
        NX_CRITICAL(sessionIter->second);
        std::swap(session, sessionIter->second->session); //< Should be destroyed outside of guard.
        m_sessionById.erase(sessionIter);
        NX_DEBUG(this, "Dropped session %1", id);
    }
    m_iceManager.freeIce(id); //< Should be unguarded.
}

void SessionPool::gotIceFromManager(const std::string& sessionId, const IceCandidate& iceCandidate)
{
    NX_ASSERT(iceCandidate.type == IceCandidate::Type::Relay ||
        iceCandidate.type == IceCandidate::Type::Srflx);

    NX_DEBUG(this, "Got ice candidate from manager: %1 -> %2", sessionId, iceCandidate.serialize());

    NX_MUTEX_LOCKER lk(&m_mutex);
    auto session = m_sessionById.find(sessionId);
    if (session == m_sessionById.end())
    {
        NX_DEBUG(this, "Session with id %1 for ice candidate %2 has already been deleted",
            sessionId, iceCandidate.serialize());
        return;
    }
    if (!session->second)
    {
        NX_ASSERT(false, "No session with id %1 stored for ice candidate %2",
            sessionId, iceCandidate.serialize());
        return;
    }
    session->second->session->gotIceFromManager(iceCandidate);
}

QnFfmpegVideoTranscoder::TranscodePolicy SessionPool::transcodePolicy() const
{
    return m_transcodePolicy;
}

} // namespace nx::webrtc
