#include "connect_session_manager.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/aio/async_channel_adapter.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>
#include <nx/casssandra/async_cassandra_connection.h>

#include "traffic_relay.h"
#include "../model/client_session_pool.h"
#include "../model/listening_peer_pool.h"
#include "../model/remote_relay_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

ConnectSessionManager::ConnectSessionManager(
    const conf::Settings& settings,
    model::ClientSessionPool* clientSessionPool,
    model::ListeningPeerPool* listeningPeerPool,
    controller::AbstractTrafficRelay* trafficRelay)
    :
    m_settings(settings),
    m_clientSessionPool(clientSessionPool),
    m_listeningPeerPool(listeningPeerPool),
    m_trafficRelay(trafficRelay),
    m_remoteRelayPool(new model::RemoteRelayPeerPool("127.0.1.1"))
{
    nx::utils::SubscriptionId subscriptionId;
    m_listeningPeerPool->peerConnectedSubscription().subscribe(
        [this](std::string peer)
        {
            m_remoteRelayPool->addPeer(peer, "This_Relay_Host")
                .then(
                    [this, peer](cf::future<bool> addPeerFuture)
                    {
                        if (addPeerFuture.get())
                        {
                            NX_VERBOSE(this, lm("Failed to add peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }
                        else
                        {
                            NX_VERBOSE(this, lm("Successfully added peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }

                        return cf::unit();
                    });
        },
        &subscriptionId);

    m_listeningPeerPoolSubscriptions.insert(subscriptionId);

    m_listeningPeerPool->peerDisconnectedSubscription().subscribe(
        [this](std::string peer)
        {
            m_remoteRelayPool->removePeer(peer, "This_Relay_Host")
                .then(
                    [this, peer](cf::future<bool> addPeerFuture)
            {
                if (addPeerFuture.get())
                {
                    NX_VERBOSE(this, lm("Failed to remove peer %1 to RemoteRelayPool")
                        .arg(peer));
                }
                else
                {
                    NX_VERBOSE(this, lm("Successfully removed peer %1 to RemoteRelayPool")
                        .arg(peer));
                }

                return cf::unit();
            });
        },
        &subscriptionId);

    m_listeningPeerPoolSubscriptions.insert(subscriptionId);
}

ConnectSessionManager::~ConnectSessionManager()
{
    // Waiting scheduled async operations completion.
    m_apiCallCounter.wait();

    std::list<RelaySession> relaySessions;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        relaySessions.swap(m_relaySessions);
    }

    for (auto& relaySession: relaySessions)
        relaySession.listeningPeerConnection->pleaseStopSync();

    for (const auto& subscriptionId: m_listeningPeerPoolSubscriptions)
        m_listeningPeerPool->peerConnectedSubscription().removeSubscription(subscriptionId);
}

void ConnectSessionManager::beginListening(
    const api::BeginListeningRequest& request,
    BeginListeningHandler completionHandler)
{
    using namespace std::placeholders;

    NX_LOGX(lm("beginListening. peerName %1").arg(request.peerName), cl_logDEBUG2);

    // TODO: #ak Using getConnectionCountByPeerName makes folowing code not atomic.
    //   That can lead to server registering more connections than was allowed.

    const auto peerConnectionCount =
        m_listeningPeerPool->getConnectionCountByPeerName(request.peerName);
    if (peerConnectionCount >=
            (std::size_t)m_settings.listeningPeer().maxPreemptiveConnectionCount)
    {
        NX_LOGX(lm("Refusing beginListening for peer %1 since there are already "
            "%2 connections with maximum of %3")
            .arg(request.peerName).arg(peerConnectionCount)
            .arg(m_settings.listeningPeer().maxPreemptiveConnectionCount),
            cl_logDEBUG2);
        completionHandler(
            api::ResultCode::preemptiveConnectionCountAtMaximum,
            api::BeginListeningResponse(),
            nx_http::ConnectionEvents());
        return;
    }

    api::BeginListeningResponse response;
    response.preemptiveConnectionCount =
        m_settings.listeningPeer().recommendedPreemptiveConnectionCount;

    nx_http::ConnectionEvents connectionEvents;
    connectionEvents.onResponseHasBeenSent =
        std::bind(&ConnectSessionManager::saveServerConnection, this,
            request.peerName, _1);

    completionHandler(api::ResultCode::ok, std::move(response), std::move(connectionEvents));
}

void ConnectSessionManager::createClientSession(
    const api::CreateClientSessionRequest& request,
    CreateClientSessionHandler completionHandler)
{
    NX_LOGX(lm("Session %1. createClientSession. targetPeerName %2")
        .arg(request.desiredSessionId).arg(request.targetPeerName), cl_logDEBUG2);

    api::CreateClientSessionResponse response;
    response.sessionTimeout =
        std::chrono::duration_cast<std::chrono::seconds>(
            m_settings.connectingPeer().connectSessionIdleTimeout);

    const auto peerName = m_listeningPeerPool->findListeningPeerByDomain(request.targetPeerName);
    if (!peerName.empty())
    {
        response.sessionId = m_clientSessionPool->addSession(request.desiredSessionId, peerName);
        completionHandler(api::ResultCode::ok, std::move(response));

        return;
    }

    m_remoteRelayPool->findRelayByDomain(request.targetPeerName)
        .then(
            [completionHandler = std::move(completionHandler), response = std::move(response),
            request, this](
                cf::future<std::string> findRelayFuture) mutable
            {
                response.redirectHost = findRelayFuture.get();
                if (response.redirectHost.empty())
                {
                    NX_VERBOSE(this, lm("Session %1. Listening peer %2 was not found")
                        .arg(request.desiredSessionId)
                        .arg(request.targetPeerName));
                    completionHandler(api::ResultCode::notFound, std::move(response));

                    return cf::unit();
                }
                completionHandler(api::ResultCode::needRedirect, std::move(response));

                return cf::unit();

            });
}

void ConnectSessionManager::connectToPeer(
    const api::ConnectToPeerRequest& request,
    ConnectToPeerHandler completionHandler)
{
    using namespace std::placeholders;

    NX_LOGX(lm("Session %1. connectToPeer").arg(request.sessionId), cl_logDEBUG2);

    std::string peerName = m_clientSessionPool->getPeerNameBySessionId(request.sessionId);
    if (peerName.empty())
    {
        NX_LOGX(lm("Session %1 is not found").arg(request.sessionId), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            nx_http::ConnectionEvents());
    }

    m_listeningPeerPool->takeIdleConnection(
        peerName,
        [this, clientSessionId = request.sessionId, peerName,
            scopedCallGuard = m_apiCallCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                api::ResultCode resultCode,
                std::unique_ptr<AbstractStreamSocket> serverConnection) mutable
        {
            onAcquiredListeningPeerConnection(
                clientSessionId,
                peerName,
                std::move(completionHandler),
                resultCode,
                std::move(serverConnection));
        });
}

void ConnectSessionManager::saveServerConnection(
    const std::string& peerName,
    nx_http::HttpServerConnection* httpConnection)
{
    m_listeningPeerPool->addConnection(
        peerName,
        httpConnection->takeSocket());
}

void ConnectSessionManager::onAcquiredListeningPeerConnection(
    const std::string& connectSessionId,
    const std::string& listeningPeerName,
    ConnectSessionManager::ConnectToPeerHandler completionHandler,
    api::ResultCode resultCode,
    std::unique_ptr<AbstractStreamSocket> listeningPeerConnection)
{
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("Session %1. Could not get listening peer %2 connection. resultCode %3")
            .arg(connectSessionId).arg(listeningPeerName)
            .arg(QnLexical::serialized(resultCode)),
            cl_logDEBUG1);
        return completionHandler(resultCode, nx_http::ConnectionEvents());
    }

    NX_LOGX(lm("Session %1. Got listening peer %2 connection")
        .arg(connectSessionId).arg(listeningPeerName), cl_logDEBUG2);

    NX_ASSERT(listeningPeerConnection);

    nx_http::ConnectionEvents connectionEvents;
    connectionEvents.onResponseHasBeenSent =
        [this, connectSessionId, listeningPeerName,
            listeningPeerConnection = std::move(listeningPeerConnection)](
                nx_http::HttpServerConnection* httpConnection) mutable
        {
            startRelaying(
                connectSessionId,
                listeningPeerName,
                std::move(listeningPeerConnection),
                httpConnection);
        };
    completionHandler(api::ResultCode::ok, std::move(connectionEvents));
}

void ConnectSessionManager::startRelaying(
    const std::string& connectSessionId,
    const std::string& listeningPeerName,
    std::unique_ptr<AbstractStreamSocket> listeningPeerConnection,
    nx_http::HttpServerConnection* httpConnection)
{
    QnMutexLocker lock(&m_mutex);

    m_relaySessions.push_back(RelaySession());
    auto relaySessionIter = --m_relaySessions.end();

    relaySessionIter->id = connectSessionId;
    relaySessionIter->clientConnection = httpConnection->takeSocket();
    relaySessionIter->listeningPeerConnection = std::move(listeningPeerConnection);
    relaySessionIter->listeningPeerName = listeningPeerName;

    sendOpenTunnelNotification(relaySessionIter);
}

void ConnectSessionManager::sendOpenTunnelNotification(
    std::list<RelaySession>::iterator relaySessionIter)
{
    using namespace std::placeholders;

    api::OpenTunnelNotification notification;
    notification.setClientEndpoint(relaySessionIter->clientConnection->getForeignAddress());
    notification.setClientPeerName(relaySessionIter->clientPeerName.c_str());
    relaySessionIter->openTunnelNotificationBuffer = notification.toHttpMessage().toString();

    relaySessionIter->listeningPeerConnection->sendAsync(
        relaySessionIter->openTunnelNotificationBuffer,
        std::bind(&ConnectSessionManager::onOpenTunnelNotificationSent, this,
            _1, _2, relaySessionIter));
}

void ConnectSessionManager::onOpenTunnelNotificationSent(
    SystemError::ErrorCode sysErrorCode,
    std::size_t /*bytesSent*/,
    std::list<RelaySession>::iterator relaySessionIter)
{
    RelaySession relaySession;

    // TODO: #ak Make lock shorter. Handle cancellation problem:
    // element from m_relaySessions is removed and destructor does not wait for this session completion.
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    relaySession = std::move(*relaySessionIter);
    m_relaySessions.erase(relaySessionIter);

    if (sysErrorCode != SystemError::noError)
    {
        NX_LOGX(lm("Session %1. Failed to send open tunnel notification to %2 (%3). %4")
            .arg(relaySession.id).arg(relaySession.listeningPeerName)
            .arg(relaySession.listeningPeerConnection->getForeignAddress())
            .arg(SystemError::toString(sysErrorCode)), cl_logDEBUG1);
        return;
    }

    relaySession.listeningPeerConnection->cancelIOSync(network::aio::etNone);
    startRelaying(std::move(relaySession));
}

void ConnectSessionManager::startRelaying(RelaySession relaySession)
{
    // TODO: #ak Get rid of adapter here when StreamSocket inherits AbstractAsyncChannel.
    auto clientSideChannel =
        network::aio::makeAsyncChannelAdapter(
            std::move(relaySession.clientConnection));
    auto listeningPeerChannel =
        network::aio::makeAsyncChannelAdapter(
            std::move(relaySession.listeningPeerConnection));

    m_trafficRelay->startRelaying(
        {std::move(clientSideChannel), relaySession.clientPeerName},
        {std::move(listeningPeerChannel), relaySession.listeningPeerName});
}

//-------------------------------------------------------------------------------------------------
// ConnectSessionManagerFactory

static ConnectSessionManagerFactory::FactoryFunc customFactoryFunc;

std::unique_ptr<AbstractConnectSessionManager> ConnectSessionManagerFactory::create(
    const conf::Settings& settings,
    model::ClientSessionPool* clientSessionPool,
    model::ListeningPeerPool* listeningPeerPool,
    controller::AbstractTrafficRelay* trafficRelay)
{
    if (customFactoryFunc)
        return customFactoryFunc(settings, clientSessionPool, listeningPeerPool, trafficRelay);
    return std::make_unique<ConnectSessionManager>(
        settings, clientSessionPool, listeningPeerPool, trafficRelay);
}

ConnectSessionManagerFactory::FactoryFunc
    ConnectSessionManagerFactory::setFactoryFunc(
        ConnectSessionManagerFactory::FactoryFunc func)
{
    customFactoryFunc.swap(func);
    return func;
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
