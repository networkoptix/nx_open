#include "relay_engine.h"

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relay/view/http_handlers.h>

namespace nx {
namespace cloud {
namespace gateway {

RelayEngine::RelayEngine(
    const relay::conf::ListeningPeer& settings,
    nx_http::server::rest::MessageDispatcher* httpMessageDispatcher)
    :
    m_listeningPeerPool(settings),
    m_listeningPeerManager(
        relay::controller::ListeningPeerManagerFactory::instance().create(
            settings,
            &m_listeningPeerPool))
{
    registerApiHandlers(httpMessageDispatcher);
}

relay::model::ListeningPeerPool& RelayEngine::listeningPeerPool()
{
    return m_listeningPeerPool;
}

void RelayEngine::registerApiHandlers(
    nx_http::server::rest::MessageDispatcher* httpMessageDispatcher)
{
    httpMessageDispatcher->registerRequestProcessor<relay::view::BeginListeningHandler>(
        relay::view::BeginListeningHandler::kPath,
        [this]() -> std::unique_ptr<relay::view::BeginListeningHandler>
        {
            return std::make_unique<relay::view::BeginListeningHandler>(
                m_listeningPeerManager.get());
        },
        nx_http::Method::post);
}

} // namespace gateway
} // namespace cloud
} // namespace nx
