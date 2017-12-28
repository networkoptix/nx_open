#include <sstream>

#include "connect_session_manager.h"

#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/aio/async_channel_adapter.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

#include "traffic_relay.h"
#include "../model/client_session_pool.h"
#include "../model/remote_relay_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

ConnectSessionManager::ConnectSessionManager(
    const conf::Settings& settings,
    model::ClientSessionPool* clientSessionPool,
    relaying::ListeningPeerPool* listeningPeerPool,
    model::AbstractRemoteRelayPeerPool* remoteRelayPeerPool,
    controller::AbstractTrafficRelay* trafficRelay)
    :
    m_settings(settings),
    m_clientSessionPool(clientSessionPool),
    m_listeningPeerPool(listeningPeerPool),
    m_remoteRelayPeerPool(remoteRelayPeerPool),
    m_trafficRelay(trafficRelay)
{
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

    m_remoteRelayPeerPool->findRelayByDomain(request.targetPeerName)
        .then(
            [completionHandler = std::move(completionHandler), response = std::move(response),
                request, this](
                    cf::future<std::string> findRelayFuture) mutable
            {
                auto redirectEndpointString = findRelayFuture.get();
                if (redirectEndpointString.empty())
                {
                    NX_VERBOSE(this, lm("Session %1. Listening peer %2 was not found")
                        .arg(request.desiredSessionId).arg(request.targetPeerName));
                    completionHandler(api::ResultCode::notFound, std::move(response));

                    return cf::unit();
                }

                std::stringstream ss;
                ss << "http://" << redirectEndpointString << "/relay/server/"
                    << request.targetPeerName << "/client_sessions/";

                response.actualRelayUrl = ss.str();
                NX_VERBOSE(this, lm("Session %1. Redirect relay %2 found for peer %3")
                    .arg(request.desiredSessionId)
                    .arg(response.actualRelayUrl)
                    .arg(request.targetPeerName));

                completionHandler(api::ResultCode::needRedirect, std::move(response));

                return cf::unit();
            });
}

void ConnectSessionManager::connectToPeer(
    const ConnectToPeerRequestEx& request,
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
            nx::network::http::ConnectionEvents());
    }

    relaying::ClientInfo clientInfo;
    clientInfo.relaySessionId = request.sessionId;
    clientInfo.endpoint = request.clientEndpoint;
    // clientInfo.peerName = ...; TODO: #ak

    m_listeningPeerPool->takeIdleConnection(
        clientInfo,
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
        return completionHandler(resultCode, nx::network::http::ConnectionEvents());
    }

    NX_LOGX(lm("Session %1. Got listening peer %2 connection")
        .arg(connectSessionId).arg(listeningPeerName), cl_logDEBUG2);

    NX_ASSERT(listeningPeerConnection);

    nx::network::http::ConnectionEvents connectionEvents;
    connectionEvents.onResponseHasBeenSent =
        [this, connectSessionId, listeningPeerName,
            listeningPeerConnection = std::move(listeningPeerConnection)](
                nx::network::http::HttpServerConnection* httpConnection) mutable
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
    nx::network::http::HttpServerConnection* httpConnection)
{
    QnMutexLocker lock(&m_mutex);

    m_relaySessions.push_back(RelaySession());
    auto relaySessionIter = --m_relaySessions.end();

    relaySessionIter->id = connectSessionId;
    relaySessionIter->clientConnection = httpConnection->takeSocket();
    relaySessionIter->listeningPeerConnection = std::move(listeningPeerConnection);
    relaySessionIter->listeningPeerName = listeningPeerName;

    auto relaySession = std::move(*relaySessionIter);
    m_relaySessions.erase(relaySessionIter);
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
    relaying::ListeningPeerPool* listeningPeerPool,
    model::AbstractRemoteRelayPeerPool* remoteRelayPeerPool,
    controller::AbstractTrafficRelay* trafficRelay)
{
    if (customFactoryFunc)
        return customFactoryFunc(settings, clientSessionPool, listeningPeerPool, trafficRelay);

    return std::make_unique<ConnectSessionManager>(
        settings,
        clientSessionPool,
        listeningPeerPool,
        remoteRelayPeerPool,
        trafficRelay);
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
