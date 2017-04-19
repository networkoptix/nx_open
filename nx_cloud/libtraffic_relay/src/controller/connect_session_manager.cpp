#include "connect_session_manager.h"

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include "traffic_relay.h"
#include "../model/client_session_pool.h"
#include "../model/listening_peer_pool.h"
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
    m_trafficRelay(trafficRelay)
{
}

ConnectSessionManager::~ConnectSessionManager()
{
    // TODO: Waiting scheduled async operations completion.
}

void ConnectSessionManager::beginListening(
    const api::BeginListeningRequest& request,
    BeginListeningHandler completionHandler)
{
    using namespace std::placeholders;

    NX_LOGX(lm("beginListening. peerName %1").arg(request.peerName), cl_logDEBUG2);

    const auto peerConnectionCount =
        m_listeningPeerPool->getConnectionCountByPeerName(request.peerName);
    if (peerConnectionCount >=
            (std::size_t)m_settings.listeningPeer().maxPreemptiveConnectionCount)
    {
        NX_LOGX(lm("Refusing beginListening for peer %1 since there are already "
            "%2 connections with maximum of %3")
            .str(request.peerName).str(peerConnectionCount)
            .str(m_settings.listeningPeer().maxPreemptiveConnectionCount),
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

    completionHandler(
        api::ResultCode::ok,
        std::move(response),
        std::move(connectionEvents));
}

void ConnectSessionManager::createClientSession(
    const api::CreateClientSessionRequest& request,
    CreateClientSessionHandler completionHandler)
{
    api::CreateClientSessionResponse response;
    response.sessionTimeout = 
        std::chrono::duration_cast<std::chrono::seconds>(
            m_settings.connectingPeer().connectSessionIdleTimeout);

    if (!m_listeningPeerPool->isPeerListening(request.targetPeerName))
    {
        NX_LOGX(lm("Received createClientSession request with unknown listening peer id %1")
            .str(request.targetPeerName), cl_logDEBUG2);
        return completionHandler(
            api::ResultCode::notFound,
            std::move(response));
    }

    response.sessionId = m_clientSessionPool->addSession(
        request.desiredSessionId, request.targetPeerName);
    completionHandler(api::ResultCode::ok, std::move(response));
}

void ConnectSessionManager::connectToPeer(
    const api::ConnectToPeerRequest& request,
    ConnectToPeerHandler completionHandler)
{
    using namespace std::placeholders;

    std::string peerName = m_clientSessionPool->getPeerNameBySessionId(request.sessionId);
    if (peerName.empty())
    {
        NX_LOGX(lm("Received connect request with unknown session id %1")
            .str(request.sessionId), cl_logDEBUG2);
        return completionHandler(
            api::ResultCode::notFound,
            nx_http::ConnectionEvents());
    }

    // TODO: Have to wait until this request completion on server stop.
    m_listeningPeerPool->takeIdleConnection(
        peerName,
        [this, clientSessionId = request.sessionId, peerName,
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
        return completionHandler(resultCode, nx_http::ConnectionEvents());

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
    const std::string& /*clientSessionId*/,
    const std::string& listeningPeerName,
    std::unique_ptr<AbstractStreamSocket> listeningPeerConnection,
    nx_http::HttpServerConnection* httpConnection)
{
    auto clientConnection = httpConnection->takeSocket();
    m_trafficRelay->startRelaying(
        {std::move(clientConnection), std::string()},
        {std::move(listeningPeerConnection), listeningPeerName});
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
