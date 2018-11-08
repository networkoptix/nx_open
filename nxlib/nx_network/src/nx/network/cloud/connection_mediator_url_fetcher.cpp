#include "connection_mediator_url_fetcher.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

void ConnectionMediatorUrlFetcher::get(Handler handler)
{
    get(nx::network::http::AuthInfo(), std::move(handler));
}

void ConnectionMediatorUrlFetcher::get(
    nx::network::http::AuthInfo auth,
    Handler handler)
{
    using namespace std::chrono;
    using namespace std::placeholders;

    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_mediatorHostDescriptor)
    {
        auto mediator = *m_mediatorHostDescriptor;
        lk.unlock();
        handler(
            nx::network::http::StatusCode::ok,
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
        m_mediatorHostDescriptor = MediatorHostDescriptor();
        m_mediatorHostDescriptor->tcpUrl = nx::utils::Url(foundTcpUrlStr);
        m_mediatorHostDescriptor->udpUrl = nx::utils::Url(foundUdpUrlStr);
        return true;
    }

    QString foundHpmUrlStr;
    if (searchResult.get(CloudInstanceSelectionAttributeNameset::hpmUrl, &foundHpmUrlStr))
    {
        m_mediatorHostDescriptor = MediatorHostDescriptor();
        m_mediatorHostDescriptor->tcpUrl = buildUrl(
            foundHpmUrlStr,
            CloudInstanceSelectionAttributeNameset::hpmUrl);
        m_mediatorHostDescriptor->udpUrl = m_mediatorHostDescriptor->tcpUrl;
        return true;
    }

    return false;
}

void ConnectionMediatorUrlFetcher::invokeHandler(
    const Handler& handler,
    nx::network::http::StatusCode::Value statusCode)
{
    NX_ASSERT(statusCode != nx::network::http::StatusCode::ok || static_cast<bool>(m_mediatorHostDescriptor));
    handler(
        statusCode,
        m_mediatorHostDescriptor ? m_mediatorHostDescriptor->tcpUrl : nx::utils::Url(),
        m_mediatorHostDescriptor ? m_mediatorHostDescriptor->udpUrl : nx::utils::Url());
}

} // namespace cloud
} // namespace network
} // namespace nx
