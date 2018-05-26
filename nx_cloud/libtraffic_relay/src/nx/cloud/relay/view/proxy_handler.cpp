#include "proxy_handler.h"

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relaying/listening_peer_connector.h>

#include "../model/abstract_remote_relay_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

ProxyHandler::ProxyHandler(
    relaying::AbstractListeningPeerPool* listeningPeerPool,
    model::AbstractRemoteRelayPeerPool* remotePeerPool)
    :
    m_listeningPeerPool(listeningPeerPool),
    m_remotePeerPool(remotePeerPool)
{
}

ProxyHandler::~ProxyHandler()
{
    m_guard.reset();
}

void ProxyHandler::detectProxyTarget(
    const nx::network::http::HttpServerConnection& connection,
    nx::network::http::Request* const request,
    ProxyTargetDetectedHandler handler)
{
    auto hostHeaderIter = request->headers.find("Host");
    if (hostHeaderIter == request->headers.end())
    {
        handler(network::http::StatusCode::badRequest, TargetHost());
        return;
    }

    m_requestUrl = request->requestLine.url;
    m_requestUrl.setScheme(
        connection.isSsl()
        ? network::http::kSecureUrlSchemeName
        : network::http::kUrlSchemeName);

    const auto targetHostNameCandidates =
        extractTargetHostNameCandidates(hostHeaderIter->second.toStdString());
    selectOnlineHost(targetHostNameCandidates, std::move(handler));
}

std::unique_ptr<network::aio::AbstractAsyncConnector>
    ProxyHandler::createTargetConnector()
{
    return std::make_unique<relaying::ListeningPeerConnector>(
        m_listeningPeerPool);
}

std::vector<std::string> ProxyHandler::extractTargetHostNameCandidates(
    const std::string& hostHeader) const
{
    constexpr int kMaxHostDepth = 2;

    std::vector<std::string> hostNames;
    hostNames.reserve(kMaxHostDepth);

    int pos = -1;
    for (int i = 0; i < kMaxHostDepth; ++i)
    {
        pos = hostHeader.find('.', pos + 1);
        if (pos == -1)
            break;

        hostNames.push_back(hostHeader.substr(0, pos));
    }

    return hostNames;
}

void ProxyHandler::selectOnlineHost(
    const std::vector<std::string>& hostNames,
    ProxyTargetDetectedHandler handler)
{
    using namespace std::placeholders;

    ProxyHandler::TargetHost result;
    result.sslMode = network::http::server::proxy::SslMode::followIncomingConnection;

    const auto host = selectOnlineHostLocally(hostNames);
    if (host)
    {
        result.target = network::SocketAddress(
            *host, network::http::DEFAULT_HTTP_PORT);
        handler(network::http::StatusCode::ok, std::move(result));
        return;
    }

    m_detectProxyTargetHandler = std::move(handler);

    findRelayInstanceToRedirectTo(
        hostNames,
        std::bind(&ProxyHandler::onRelayInstanceSearchCompleted, this, _1, _2));
}

std::optional<std::string> ProxyHandler::selectOnlineHostLocally(
    const std::vector<std::string>& candidates) const
{
    for (const auto& host: candidates)
    {
        if (m_listeningPeerPool->isPeerOnline(host))
            return host;
    }

    return std::nullopt;
}

void ProxyHandler::findRelayInstanceToRedirectTo(
    const std::vector<std::string>& hostNames,
    nx::utils::MoveOnlyFunc<void(
        std::optional<std::string> /*relayHostName*/,
        std::string /*proxyTargetHostName*/)> handler)
{
    m_pendingRequestCount = hostNames.size();

    for (const auto& domainName: hostNames)
    {
        m_remotePeerPool->findRelayByDomain(domainName).then(
            [this, domainName, sharedGuard = m_guard.sharedGuard()](
                cf::future<std::string> relayDomainFuture)
            {
                auto lock = sharedGuard->lock();
                if (!lock)
                    return cf::unit();

                --m_pendingRequestCount;
                if (m_hostDetected)
                    return cf::unit();

                auto relayDomain = relayDomainFuture.get();
                if (relayDomain.empty())
                {
                    if (m_pendingRequestCount == 0)
                        onRelayInstanceSearchCompleted(std::nullopt, std::string());
                    // Else, waiting for other requests to finish.
                    return cf::unit();
                }

                m_hostDetected = true;
                onRelayInstanceSearchCompleted(relayDomain, domainName);
                return cf::unit();
            });
    }
}

void ProxyHandler::onRelayInstanceSearchCompleted(
    std::optional<std::string> relayHost,
    const std::string& proxyTargetHost)
{
    if (!relayHost)
    {
        nx::utils::swapAndCall(
            m_detectProxyTargetHandler,
            network::http::StatusCode::badGateway,
            TargetHost());
        return;
    }

    auto locationUrl = network::url::Builder(m_requestUrl)
        .setHost(lm("%1.%2").args(proxyTargetHost, *relayHost)).toUrl();

    response()->headers.emplace("Location", locationUrl.toString().toUtf8());

    nx::utils::swapAndCall(
        m_detectProxyTargetHandler,
        network::http::StatusCode::found,
        TargetHost());
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
