#include "discovery_http_server.h"

#include "registered_peer_pool.h"
#include "discovery_http_api_path.h"

namespace nx {
namespace cloud {
namespace discovery {

HttpServer::HttpServer(
    nx_http::server::rest::MessageDispatcher* httpMessageDispatcher,
    RegisteredPeerPool* registeredPeerPool)
    :
    m_httpMessageDispatcher(httpMessageDispatcher),
    m_registeredPeerPool(registeredPeerPool)
{
    //m_httpMessageDispatcher->registerRequestProcessor(
    //    http::kDiscoveredModulesPath,
    //    [](){},
    //    nx_http::Method::get);
}

} // namespace nx
} // namespace cloud
} // namespace discovery
