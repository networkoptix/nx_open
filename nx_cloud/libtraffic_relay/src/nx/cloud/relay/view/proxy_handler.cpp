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
    const nx::network::http::HttpServerConnection& /*connection*/,
    nx::network::http::Request* const request)
{
    ProxyHandler::TargetHost result;
    result.sslMode = network::http::server::proxy::SslMode::followIncomingConnection;

    auto hostHeaderIter = request->headers.find("Host");
    if (hostHeaderIter == request->headers.end())
    {
        result.status = network::http::StatusCode::badRequest;
        return result;
    }

    const auto host = extractOnlineHostName(
        hostHeaderIter->second.toStdString());
    if (host.empty())
    {
        result.status = network::http::StatusCode::badGateway;
        return result;
    }

    result.target = network::SocketAddress(
        host, network::http::DEFAULT_HTTP_PORT);
    result.status = network::http::StatusCode::ok;
    return result;
}

std::unique_ptr<network::aio::AbstractAsyncConnector>
    ProxyHandler::createTargetConnector()
{
    return std::make_unique<relaying::ListeningPeerConnector>(
        m_listeningPeerPool);
}

std::string ProxyHandler::extractOnlineHostName(
    const std::string& hostHeader)
{
    constexpr int kMaxHostDepth = 2;

    int pos = -1;
    for (int i = 0; i < kMaxHostDepth; ++i)
    {
        pos = hostHeader.find('.', pos + 1);
        if (pos == -1)
            break;

        const auto host = hostHeader.substr(0, pos);
        if (m_listeningPeerPool->isPeerOnline(host))
            return host;
    }

    return std::string();
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
