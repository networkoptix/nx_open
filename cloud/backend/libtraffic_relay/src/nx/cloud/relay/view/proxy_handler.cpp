#include "proxy_handler.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/relaying/listening_peer_connector.h>

#include "../model/remote_relay_peer_pool_aio_wrapper.h"
#include "../model/alias_manager.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

ProxyHandler::ProxyHandler(
    const conf::Settings& settings,
    relaying::AbstractListeningPeerPool* listeningPeerPool,
    model::RemoteRelayPeerPoolAioWrapper* remotePeerPool,
    model::AliasManager* aliasManager)
    :
    m_listeningPeerPool(listeningPeerPool),
    m_remotePeerPool(remotePeerPool),
    m_aliasManager(aliasManager)
{
    if (settings.https().sslHandshakeTimeout)
        setSslHandshakeTimeout(*settings.https().sslHandshakeTimeout);
}

ProxyHandler::~ProxyHandler()
{
    m_guard.reset();
}

void ProxyHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    base_type::processRequest(
        std::move(requestContext),
        [this, completionHandler = std::move(completionHandler)](
            network::http::RequestResult requestResult)
        {
            nx::network::http::insertOrReplaceHeader(
                &this->response()->headers,
                nx::network::http::HttpHeader("Access-Control-Allow-Origin", "*"));
            completionHandler(std::move(requestResult));
        });
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

    if (auto hostName = findHostByAlias(targetHostNameCandidates))
    {
        handler(network::http::StatusCode::ok, TargetHost(*hostName));
        return;
    }

    if (auto hostName = selectOnlineHostLocally(targetHostNameCandidates))
    {
        m_targetHostAlias = m_aliasManager->addAliasToHost(std::string());
        hostHeaderIter->second.replace(hostName->c_str(), m_targetHostAlias.c_str());
        handler(network::http::StatusCode::ok, TargetHost(*hostName));
        return;
    }

    findRelayInstanceToRedirectTo(targetHostNameCandidates, std::move(handler));
}

std::unique_ptr<network::aio::AbstractAsyncConnector>
    ProxyHandler::createTargetConnector()
{
    auto connector = std::make_unique<relaying::ListeningPeerConnector>(
        m_listeningPeerPool);
    connector->setOnSuccessfulConnect(
        [alias = m_targetHostAlias, aliasManager = m_aliasManager](
            const std::string& actualHostName)
        {
            aliasManager->updateAlias(alias, actualHostName);
        });
    return connector;
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

std::optional<std::string> ProxyHandler::findHostByAlias(
    const std::vector<std::string>& possibleAliases)
{
    for (const auto& value: possibleAliases)
    {
        if (const auto hostName = m_aliasManager->findHostByAlias(value); hostName)
        {
            NX_VERBOSE(this, lm("Using alias %1 for host %2").args(value, *hostName));
            return hostName;
        }
    }

    return std::nullopt;
}

void ProxyHandler::findRelayInstanceToRedirectTo(
    const std::vector<std::string>& hostNames,
    ProxyTargetDetectedHandler handler)
{
    using namespace std::placeholders;

    m_detectProxyTargetHandler = std::move(handler);

    if (m_remotePeerPool->isConnected())
    {
        invokeRemotePeerPool(
            hostNames,
            std::bind(&ProxyHandler::onRelayInstanceSearchCompleted, this, _1, _2));
    }
    else
    {
        onRelayInstanceSearchCompleted(std::nullopt, std::string());
    }
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

void ProxyHandler::invokeRemotePeerPool(
    const std::vector<std::string>& hostNames,
    nx::utils::MoveOnlyFunc<void(
        std::optional<std::string> /*relayHostName*/,
        std::string /*proxyTargetHostName*/)> handler)
{
    QnMutexLocker lock(&m_mutex);

    m_pendingRequestCount = hostNames.size();
    m_findRelayInstanceHandler = std::move(handler);

    for (const auto& domainName: hostNames)
    {
        m_remotePeerPool->findRelayByDomain(
            domainName,
            [this, domainName, sharedGuard = m_guard.sharedGuard()](
                std::string relayDomain)
            {
                auto lock = sharedGuard->lock();
                if (!lock)
                    return;

                {
                    // Using this to make sure
                    // ProxyHandler::findRelayInstanceToRedirectTo has exited.
                    QnMutexLocker lock(&m_mutex);
                }

                processFindRelayResult(relayDomain, domainName);
            });
    }
}

void ProxyHandler::processFindRelayResult(
    const std::string& relayDomain,
    const std::string& proxyTarget)
{
    --m_pendingRequestCount;
    if (!m_findRelayInstanceHandler)
        return;

    if (relayDomain.empty())
    {
        if (m_pendingRequestCount == 0)
        {
            nx::utils::swapAndCall(
                m_findRelayInstanceHandler, std::nullopt, std::string());
        }

        // Else, waiting for other requests to finish.
        return;
    }

    nx::utils::swapAndCall(
        m_findRelayInstanceHandler, relayDomain, proxyTarget);
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
        .setEndpoint(network::SocketAddress(
            lm("%1.%2").args(proxyTargetHost, *relayHost))).toUrl();

    response()->headers.emplace("Location", locationUrl.toString().toUtf8());

    nx::utils::swapAndCall(
        m_detectProxyTargetHandler,
        network::http::StatusCode::temporaryRedirect,
        TargetHost());
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
