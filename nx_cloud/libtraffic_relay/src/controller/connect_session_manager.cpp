#include "connect_session_manager.h"

#include <nx/utils/log/log.h>

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
    model::ListeningPeerPool* listeningPeerPool)
    :
    m_settings(settings),
    m_clientSessionPool(clientSessionPool),
    m_listeningPeerPool(listeningPeerPool)
{
}

void ConnectSessionManager::beginListening(
    const api::BeginListeningRequest& request,
    BeginListeningHandler completionHandler)
{
    using namespace std::placeholders;

    NX_LOGX(lm("beginListening. peerName %1").arg(request.peerName), cl_logDEBUG2);

    if (m_listeningPeerPool->getConnectionCountByPeerName(request.peerName) >=
        (std::size_t)m_settings.listeningPeer().maxPreemptiveConnectionCount)
    {
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
    const api::CreateClientSessionRequest& /*request*/,
    CreateClientSessionHandler /*completionHandler*/)
{
    // TODO
}

void ConnectSessionManager::connectToPeer(
    const api::ConnectToPeerRequest& /*request*/,
    ConnectToPeerHandler /*completionHandler*/)
{
    // TODO
}

void ConnectSessionManager::saveServerConnection(
    const std::string& peerName,
    nx_http::HttpServerConnection* httpConnection)
{
    m_listeningPeerPool->addConnection(
        peerName,
        httpConnection->takeSocket());
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
