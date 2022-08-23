// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_mediator_url_fetcher.h"

#include <nx/utils/log/log.h>

namespace nx::network::cloud {

void ConnectionMediatorUrlFetcher::get(Handler handler)
{
    get(nx::network::http::AuthInfo(), ssl::kDefaultCertificateCheck, std::move(handler));
}

void ConnectionMediatorUrlFetcher::get(
    nx::network::http::AuthInfo auth, nx::network::ssl::AdapterFunc adapterFunc, Handler handler)
{
    // If requested endpoint is known, providing it to the output.
    NX_MUTEX_LOCKER lk(&m_mutex);

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

    initiateModulesXmlRequestIfNeeded(auth, std::move(adapterFunc), std::move(handler));
}

bool ConnectionMediatorUrlFetcher::analyzeXmlSearchResult(
    const nx::utils::stree::AttributeDictionary& searchResult)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    auto foundTcpUrlStr = searchResult.get<std::string>(CloudInstanceSelectionAttributeNameset::hpmTcpUrl);
    auto foundUdpUrlStr = searchResult.get<std::string>(CloudInstanceSelectionAttributeNameset::hpmUdpUrl);
    if (foundTcpUrlStr && foundUdpUrlStr)
    {
        m_mediatorHostDescriptor = MediatorHostDescriptor();
        m_mediatorHostDescriptor->tcpUrl = nx::utils::Url(*foundTcpUrlStr);
        m_mediatorHostDescriptor->udpUrl = nx::utils::Url(*foundUdpUrlStr);
        return true;
    }

    auto foundHpmUrlStr = searchResult.get<std::string>(CloudInstanceSelectionAttributeNameset::hpmUrl);
    if (foundHpmUrlStr)
    {
        m_mediatorHostDescriptor = MediatorHostDescriptor();
        m_mediatorHostDescriptor->tcpUrl = buildUrl(
            *foundHpmUrlStr,
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
    NX_ASSERT(
        statusCode != nx::network::http::StatusCode::ok ||
        static_cast<bool>(m_mediatorHostDescriptor));

    handler(
        statusCode,
        m_mediatorHostDescriptor ? m_mediatorHostDescriptor->tcpUrl : nx::utils::Url(),
        m_mediatorHostDescriptor ? m_mediatorHostDescriptor->udpUrl : nx::utils::Url());
}

} // namespace nx::network::cloud
