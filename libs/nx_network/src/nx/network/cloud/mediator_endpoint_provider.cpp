#include "mediator_endpoint_provider.h"

#include <nx/network/url/url_parse_helper.h>

namespace nx::hpm::api {

MediatorEndpointProvider::MediatorEndpointProvider(const std::string& cloudHost):
    m_cloudHost(cloudHost)
{
}

void MediatorEndpointProvider::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_mediatorUrlFetcher)
        m_mediatorUrlFetcher->bindToAioThread(aioThread);
}

void MediatorEndpointProvider::mockupCloudModulesXmlUrl(
    const nx::utils::Url& cloudModulesXmlUrl)
{
    m_cloudModulesXmlUrl = cloudModulesXmlUrl;
}

void MediatorEndpointProvider::mockupMediatorAddress(
    const MediatorAddress& mediatorAddress)
{
    QnMutexLocker lock(&m_mutex);

    m_mediatorAddress = mediatorAddress;
}

void MediatorEndpointProvider::fetchMediatorEndpoints(
    FetchMediatorEndpointsCompletionHandler handler)
{
    NX_ASSERT(isInSelfAioThread());

    m_fetchMediatorEndpointsHandlers.push_back(std::move(handler));

    if (m_mediatorUrlFetcher)
        return; //< Operation has already been started.

    initializeUrlFetcher();

    m_mediatorUrlFetcher->get(
        [this](
            nx::network::http::StatusCode::Value resultCode,
            nx::utils::Url tcpUrl,
            nx::utils::Url udpUrl)
        {
            m_mediatorUrlFetcher.reset();

            if (nx::network::http::StatusCode::isSuccessCode(resultCode))
            {
                NX_DEBUG(this, lm("Fetched mediator tcp (%1) and udp (%2) URLs")
                    .args(tcpUrl, udpUrl));

                QnMutexLocker lock(&m_mutex);
                m_mediatorAddress = MediatorAddress{
                    tcpUrl,
                    nx::network::url::getEndpoint(udpUrl) };
            }
            else
            {
                NX_DEBUG(this, lm("Cannot fetch mediator address. HTTP %1")
                    .arg((int)resultCode));
            }

            for (auto& handler : m_fetchMediatorEndpointsHandlers)
                nx::utils::swapAndCall(handler, resultCode);
            m_fetchMediatorEndpointsHandlers.clear();
        });
}

std::optional<MediatorAddress> MediatorEndpointProvider::mediatorAddress() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mediatorAddress;
}

void MediatorEndpointProvider::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_mediatorUrlFetcher.reset();
}

void MediatorEndpointProvider::initializeUrlFetcher()
{
    m_mediatorUrlFetcher =
        std::make_unique<nx::network::cloud::ConnectionMediatorUrlFetcher>();
    m_mediatorUrlFetcher->bindToAioThread(getAioThread());

    if (m_cloudModulesXmlUrl)
    {
        m_mediatorUrlFetcher->setModulesXmlUrl(*m_cloudModulesXmlUrl);
    }
    else
    {
        m_mediatorUrlFetcher->setModulesXmlUrl(
            network::AppInfo::defaultCloudModulesXmlUrl(m_cloudHost.c_str()));
    }
}

} // namespace nx::hpm::api
