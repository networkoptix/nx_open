#include "relay_engine.h"

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>

#include "http_view/begin_listening_http_handler.h"

namespace nx {
namespace cloud {
namespace relaying {

RelayEngine::RelayEngine(
    const Settings& settings,
    nx_http::server::rest::MessageDispatcher* httpMessageDispatcher)
    :
    m_listeningPeerPool(settings),
    m_listeningPeerManager(
        ListeningPeerManagerFactory::instance().create(
            settings,
            &m_listeningPeerPool))
{
    registerApiHandlers(httpMessageDispatcher);
}

ListeningPeerPool& RelayEngine::listeningPeerPool()
{
    return m_listeningPeerPool;
}

void RelayEngine::registerApiHandlers(
    nx_http::server::rest::MessageDispatcher* httpMessageDispatcher)
{
    httpMessageDispatcher->registerRequestProcessor<BeginListeningHandler>(
        BeginListeningHandler::kPath,
        [this]() -> std::unique_ptr<BeginListeningHandler>
        {
            return std::make_unique<BeginListeningHandler>(m_listeningPeerManager.get());
        },
        nx_http::Method::post);
}

} // namespace relaying
} // namespace cloud
} // namespace nx
