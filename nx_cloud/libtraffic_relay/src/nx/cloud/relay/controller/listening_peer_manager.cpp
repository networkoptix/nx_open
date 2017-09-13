#include "listening_peer_manager.h"

#include <nx/utils/log/log.h>

#include "../model/listening_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

ListeningPeerManager::ListeningPeerManager(
    const conf::ListeningPeer& settings,
    model::ListeningPeerPool* listeningPeerPool)
    :
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool)
{
}

void ListeningPeerManager::beginListening(
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
        (std::size_t)m_settings.maxPreemptiveConnectionCount)
    {
        NX_LOGX(lm("Refusing beginListening for peer %1 since there are already "
            "%2 connections with maximum of %3")
            .arg(request.peerName).arg(peerConnectionCount)
            .arg(m_settings.maxPreemptiveConnectionCount),
            cl_logDEBUG2);
        completionHandler(
            api::ResultCode::preemptiveConnectionCountAtMaximum,
            api::BeginListeningResponse(),
            nx_http::ConnectionEvents());
        return;
    }

    api::BeginListeningResponse response;
    response.preemptiveConnectionCount =
        m_settings.recommendedPreemptiveConnectionCount;

    nx_http::ConnectionEvents connectionEvents;
    connectionEvents.onResponseHasBeenSent =
        std::bind(&ListeningPeerManager::saveServerConnection, this,
            request.peerName, _1);

    completionHandler(
        api::ResultCode::ok,
        std::move(response),
        std::move(connectionEvents));
}

void ListeningPeerManager::saveServerConnection(
    const std::string& peerName,
    nx_http::HttpServerConnection* httpConnection)
{
    m_listeningPeerPool->addConnection(
        peerName,
        httpConnection->takeSocket());
}

//-------------------------------------------------------------------------------------------------

using namespace std::placeholders;

ListeningPeerManagerFactory::ListeningPeerManagerFactory():
    base_type(std::bind(&ListeningPeerManagerFactory::defaultFactoryFunction, this, _1, _2))
{
}

ListeningPeerManagerFactory& ListeningPeerManagerFactory::instance()
{
    static ListeningPeerManagerFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractListeningPeerManager> ListeningPeerManagerFactory::defaultFactoryFunction(
    const conf::ListeningPeer& settings,
    model::ListeningPeerPool* listeningPeerPool)
{
    return std::make_unique<ListeningPeerManager>(
        settings,
        listeningPeerPool);
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
