#include "relay_engine.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>

#include "http_view/begin_listening_http_handler.h"

namespace nx {
namespace cloud {
namespace relaying {

RelayEngine::RelayEngine(
    const Settings& settings,
    nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher)
    :
    m_listeningPeerPool(settings),
    m_listeningPeerManager(
        ListeningPeerManagerFactory::instance().create(
            settings,
            &m_listeningPeerPool)),
    m_tunnelingServer(m_listeningPeerManager.get(), &m_listeningPeerPool)
{
    registerApiHandlers(httpMessageDispatcher);
}

ListeningPeerPool& RelayEngine::listeningPeerPool()
{
    return m_listeningPeerPool;
}

void RelayEngine::registerApiHandlers(
    nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher)
{
    m_tunnelingServer.registerHandlers(
        relay::api::kServerTunnelBasePath,
        httpMessageDispatcher);
}

} // namespace relaying
} // namespace cloud
} // namespace nx
