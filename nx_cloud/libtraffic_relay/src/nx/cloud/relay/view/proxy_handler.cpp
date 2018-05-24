#include "proxy_handler.h"

#include <nx/cloud/relaying/listening_peer_connector.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

ProxyHandler::ProxyHandler(
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_listeningPeerPool(listeningPeerPool)
{
}

ProxyHandler::TargetHost ProxyHandler::cutTargetFromRequest(
    const nx::network::http::HttpServerConnection& connection,
    nx::network::http::Request* const request)
{
    ProxyHandler::TargetHost result;

    auto hostHeaderIter = request->headers.find("Host");
    if (hostHeaderIter == request->headers.end())
    {
        result.status = network::http::StatusCode::badRequest;
        return result;
    }

    // TODO: #ak Following check is not valid.
    const auto host = hostHeaderIter->second.split('.').front();

    result.target = network::SocketAddress(
        host.constData(), network::http::DEFAULT_HTTP_PORT);
    result.status = network::http::StatusCode::ok;
    return result;
}

std::unique_ptr<network::aio::AbstractAsyncConnector>
    ProxyHandler::createTargetConnector()
{
    return std::make_unique<relaying::ListeningPeerConnector>(
        m_listeningPeerPool);
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
