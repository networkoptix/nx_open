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

void ProxyHandler::detectProxyTarget(
    const nx::network::http::HttpServerConnection& /*connection*/,
    nx::network::http::Request* const request,
    ProxyTargetDetectedHandler handler)
{
    ProxyHandler::TargetHost result;
    result.sslMode = network::http::server::proxy::SslMode::followIncomingConnection;

    auto hostHeaderIter = request->headers.find("Host");
    if (hostHeaderIter == request->headers.end())
    {
        handler(network::http::StatusCode::badRequest, TargetHost());
        return;
    }

    const auto host = extractOnlineHostName(
        hostHeaderIter->second.toStdString());
    if (host.empty())
    {
        handler(network::http::StatusCode::badGateway, TargetHost());
        return;
    }

    result.target = network::SocketAddress(
        host, network::http::DEFAULT_HTTP_PORT);
    handler(network::http::StatusCode::ok, std::move(result));
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
