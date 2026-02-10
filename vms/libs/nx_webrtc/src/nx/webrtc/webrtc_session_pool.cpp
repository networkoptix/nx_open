// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_session_pool.h"

#include "webrtc_ice_tcp.h"
#include "webrtc_session.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::webrtc {

SessionPool::SessionPool(
    nx::vms::common::SystemContext* context,
    nx::utils::TimerManager* timerManager,
    QnFfmpegVideoTranscoder::TranscodePolicy transcodePolicy,
    const std::vector<nx::network::SocketAddress>& defaultStunServers)
    :
    nx::vms::common::SystemContextAware(context),
    m_timerManager(timerManager),
    m_turnInfoFetcher(context->globalSettings()),
    m_transcodePolicy(transcodePolicy),
    m_defaultStunServers(defaultStunServers)
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

SessionPool::~SessionPool()
{
    stop();
}

void SessionPool::stop()
{
    m_timerManager->joinAndDeleteTimer(m_timerId);
    m_turnInfoFetcher.stop();

    // TODO move working ice to session and use simple crear() without swap and deletion order.
    // Now it should be deleted before sessions, since ices have pointer to session.
    decltype(m_tcpIces) icesTcp;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        std::swap(icesTcp, m_tcpIces);
    }
    icesTcp.clear();

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sessionById.clear();
}

std::vector<nx::network::SocketAddress> SessionPool::getStunServers()
{
    auto connectionMediatorAddress = SocketGlobals::cloud().mediatorConnector().address();

    std::vector<nx::network::SocketAddress> result;
    if (connectionMediatorAddress)
        result.push_back(connectionMediatorAddress->stunUdpEndpoint);
    result.insert(result.end(), m_defaultStunServers.begin(), m_defaultStunServers.end());
    return result;
}

TurnInfoFetcher& SessionPool::turnInfoFetcher()
{
  return m_turnInfoFetcher;
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

    m_sessionById[localUfrag] = nullptr;
    return localUfrag;
}

bool SessionPool::takeUfrag(const std::string& ufrag)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    if (m_sessionById.count(ufrag) > 0)
        return false;

    m_sessionById[ufrag] = nullptr;
    return true;
}

std::shared_ptr<Session> SessionPool::createInternal(
    std::unique_ptr<AbstractCameraDataProvider> cameraProvider,
    Purpose purpose,
    const nx::network::SocketAddress& address,
    const std::string& localUfrag,
    const SessionConfig& config)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    const auto sessionIter = m_sessionById.find(localUfrag);
    if (!NX_ASSERT(sessionIter != m_sessionById.end() && !sessionIter->second,
        "Session ufrag should be taken before by takeUfrag() of emitUfrag()"))
    {
        return nullptr;
    }

    auto server = systemContext()->resourcePool()->getResourceById<QnMediaServerResource>(
        systemContext()->peerId());

    if (!server)
        return nullptr;

    auto allAddresses = server->getAllAvailableAddresses();

    std::shared_ptr<Session> session = std::make_shared<Session>(
        systemContext(),
        this,
        std::move(cameraProvider),
        localUfrag,
        address,
        allAddresses,
        config,
        getStunServers(),
        purpose);

    m_sessionById[session->id()] = session;

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

std::weak_ptr<Session> SessionPool::getSession(const std::string& id)
{
    // TODO: Maybe it is better to cleanup now unused Ices here...
    NX_MUTEX_LOCKER lk(&m_mutex);
    auto session = m_sessionById.find(id);
    if (session == m_sessionById.end() || !session->second)
        return std::weak_ptr<Session>();

    return session->second;
}

void SessionPool::onTimer(const quint64& /*timerId*/)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        for (auto it = m_sessionById.begin(); it != m_sessionById.end();)
        {
            auto& ctx = it->second;
            if (ctx && ctx->hasExpired(m_keepAliveTimeout))
            {
                NX_DEBUG(this, "Remove session on timer: %1", ctx);
                it = m_sessionById.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Clean finished TCP Ices.
        m_tcpIces.remove_if([](std::unique_ptr<IceTcp>& ice) {
            return ice->isStopped();
        });
    }
}

void SessionPool::drop(const std::string& id)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto sessionIter = m_sessionById.find(id);
        if (sessionIter == m_sessionById.end())
            return; //< Already dropped by some way.
        NX_CRITICAL(sessionIter->second);
        m_sessionById.erase(sessionIter);
        NX_DEBUG(this, "Dropped session %1", id);
    }
}

void SessionPool::gotIceTcp(std::unique_ptr<IceTcp>&& iceTcp)
{
    iceTcp->start();

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_tcpIces.push_back(std::move(iceTcp));
}

QnFfmpegVideoTranscoder::TranscodePolicy SessionPool::transcodePolicy() const
{
    return m_transcodePolicy;
}

} // namespace nx::webrtc
