#include "connection_mediator_url_fetcher.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

void ConnectionMediatorUrlFetcher::get(Handler handler)
{
    get(nx_http::AuthInfo(), std::move(handler));
}

void ConnectionMediatorUrlFetcher::get(
    nx_http::AuthInfo auth,
    Handler handler)
{
    using namespace std::chrono;
    using namespace std::placeholders;

    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_mediator)
    {
        auto mediator = *m_mediator;
        lk.unlock();
        handler(
            nx_http::StatusCode::ok,
            std::move(mediator.tcpUrl),
            std::move(mediator.udpUrl));
        return;
    }

    initiateModulesXmlRequestIfNeeded(auth, std::move(handler));
}

bool ConnectionMediatorUrlFetcher::analyzeXmlSearchResult(
    const nx::utils::stree::ResourceContainer& searchResult)
{
    QString foundTcpUrlStr;
    QString foundUdpUrlStr;
    if (searchResult.get(CloudInstanceSelectionAttributeNameset::hpmTcpUrl, &foundTcpUrlStr) &&
        searchResult.get(CloudInstanceSelectionAttributeNameset::hpmUdpUrl, &foundUdpUrlStr))
    {
        m_mediator = Mediator();
        m_mediator->tcpUrl = QUrl(foundTcpUrlStr);
        m_mediator->udpUrl = QUrl(foundUdpUrlStr);
        return true;
    }

    QString foundHpmUrlStr;
    if (searchResult.get(CloudInstanceSelectionAttributeNameset::hpmUrl, &foundHpmUrlStr))
    {
        m_mediator = Mediator();
        m_mediator->tcpUrl = buildUrl(
            foundHpmUrlStr,
            CloudInstanceSelectionAttributeNameset::hpmUrl);
        m_mediator->udpUrl = m_mediator->tcpUrl;
        return true;
    }

    return false;
}

void ConnectionMediatorUrlFetcher::invokeHandler(
    const Handler& handler,
    nx_http::StatusCode::Value statusCode)
{
    NX_ASSERT(statusCode != nx_http::StatusCode::ok || static_cast<bool>(m_mediator));
    handler(
        statusCode,
        m_mediator ? m_mediator->tcpUrl : QUrl(),
        m_mediator ? m_mediator->udpUrl : QUrl());
}

} // namespace cloud
} // namespace network
} // namespace nx
