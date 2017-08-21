#include "discovery_http_server.h"

#include "registered_peer_pool.h"

namespace nx {
namespace cloud {
namespace discovery {

HttpServer::HttpServer(
    nx_http::MessageDispatcher* httpMessageDispatcher,
    RegisteredPeerPool* registeredPeerPool)
    :
    m_httpMessageDispatcher(httpMessageDispatcher),
    m_registeredPeerPool(registeredPeerPool)
{
}

} // namespace nx
} // namespace cloud
} // namespace discovery
