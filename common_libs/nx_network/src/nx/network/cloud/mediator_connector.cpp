#include "mediator_connector.h"

#include <nx/network/socket_factory.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "connection_mediator_url_fetcher.h"
#include "mediator/api/mediator_api_http_paths.h"

namespace nx {
namespace hpm {
namespace api {

namespace { static network::stun::AbstractAsyncClient::Settings s_stunClientSettings; }

MediatorConnector::MediatorConnector(const std::string& cloudHost):
    m_cloudHost(cloudHost),
    m_fetchEndpointRetryTimer(
        std::make_unique<nx::network::RetryTimer>(
            s_stunClientSettings.reconnectPolicy))
{
    // Reconnect to mediator is handled by this class, not by STUN client.
    auto stunClientSettings = s_stunClientSettings;
    stunClientSettings.reconnectPolicy = network::RetryPolicy::kNoRetries;
    m_stunClient = std::make_shared<network::stun::AsyncClientWithHttpTunneling>(
        stunClientSettings);

    bindToAioThread(getAioThread());

    m_stunClient->setOnConnectionClosedHandler(
        std::bind(&MediatorConnector::reconnectToMediator, this));
}

MediatorConnector::~MediatorConnector()
{
    NX_ASSERT((m_stunClient == nullptr) || m_stunClient.use_count() == 1);
    pleaseStopSync();
}

void MediatorConnector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_stunClient->bindToAioThread(aioThread);
    if (m_mediatorUrlFetcher)
        m_mediatorUrlFetcher->bindToAioThread(aioThread);
    m_fetchEndpointRetryTimer->bindToAioThread(aioThread);
}

void MediatorConnector::enable(bool waitComplete)
{
    bool needToFetch = false;
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_promise)
        {
            needToFetch = true;
            m_promise = nx::utils::promise< bool >();
            m_future = m_promise->get_future();
        }
    }

    if (needToFetch)
        post([this]() { connectToMediatorAsync(); });

    if (waitComplete)
        m_future->wait();
}

std::unique_ptr<MediatorClientTcpConnection> MediatorConnector::clientConnection()
{
    return std::make_unique<MediatorClientTcpConnection>(m_stunClient);
}

std::unique_ptr<MediatorServerTcpConnection> MediatorConnector::systemConnection()
{
    return std::make_unique<MediatorServerTcpConnection>(m_stunClient, this);
}

void MediatorConnector::mockupCloudModulesXmlUrl(
    const nx::utils::Url& cloudModulesXmlUrl)
{
    m_cloudModulesXmlUrl = cloudModulesXmlUrl;
}

void MediatorConnector::mockupMediatorUrl(
    const nx::utils::Url& mediatorUrl,
    const network::SocketAddress stunUdpEndpoint)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_promise && mediatorUrl == m_mediatorUrl)
            return;

        NX_ASSERT(!m_promise, Q_FUNC_INFO,
            "Address resolving is already in progress!");

        m_promise = nx::utils::promise<bool>();
        m_future = m_promise->get_future();

        NX_DEBUG(this, lm("Mediator address is mocked up: %1").arg(mediatorUrl));

        m_mediatorUrl = mediatorUrl;
        m_mockedUpMediatorUrl = mediatorUrl;
        m_mediatorUdpEndpoint = stunUdpEndpoint;
    }   

    m_promise->set_value(true);
    establishTcpConnectionToMediatorAsync();
    if (m_mediatorAvailabilityChangedHandler)
        m_mediatorAvailabilityChangedHandler(true);
}

void MediatorConnector::setSystemCredentials(std::optional<SystemCredentials> value)
{
    bool needToReconnect = false;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_credentials == value)
            return;

        needToReconnect = static_cast<bool>(m_credentials);
        m_credentials = std::move(value);
    }

    if (needToReconnect)
        m_stunClient->closeConnection(SystemError::connectionReset);
}

std::optional<SystemCredentials> MediatorConnector::getSystemCredentials() const
{
    QnMutexLocker lock(&m_mutex);
    return m_credentials;
}

void MediatorConnector::fetchUdpEndpoint(
    nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        nx::network::SocketAddress /*endpoint*/)> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            if (m_mediatorUdpEndpoint)
                return handler(nx::network::http::StatusCode::ok, *m_mediatorUdpEndpoint);

            fetchMediatorEndpoints(
                [this, handler = std::move(handler)](
                    nx::network::http::StatusCode::Value resultCode)
                {
                    if (nx::network::http::StatusCode::isSuccessCode(resultCode))
                        handler(resultCode, *m_mediatorUdpEndpoint);
                    else
                        handler(resultCode, nx::network::SocketAddress());
                });
        });
}

std::optional<nx::network::SocketAddress> MediatorConnector::udpEndpoint() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mediatorUdpEndpoint;
}

void MediatorConnector::setOnMediatorAvailabilityChanged(
    MediatorAvailabilityChangedHandler handler)
{
    m_mediatorAvailabilityChangedHandler.swap(handler);
}

void MediatorConnector::setStunClientSettings(
    network::stun::AbstractAsyncClient::Settings stunClientSettings)
{
    s_stunClientSettings = std::move(stunClientSettings);
}

static bool isReady(const nx::utils::future<bool>& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void MediatorConnector::stopWhileInAioThread()
{
    m_stunClient.reset();
    m_mediatorUrlFetcher.reset();
    m_fetchEndpointRetryTimer.reset();
}

void MediatorConnector::connectToMediatorAsync()
{
    fetchMediatorEndpoints(
        [this](nx::network::http::StatusCode::Value resultCode)
        {
            if (!nx::network::http::StatusCode::isSuccessCode(resultCode))
            {
                if (!isReady(*m_future))
                    m_promise->set_value(false);

                // Retry after some delay.
                m_fetchEndpointRetryTimer->scheduleNextTry(
                    [this]() { connectToMediatorAsync(); });
            }
            else
            {
                establishTcpConnectionToMediatorAsync();
                if (m_mediatorAvailabilityChangedHandler)
                    m_mediatorAvailabilityChangedHandler(true);
            }
        });
}

void MediatorConnector::fetchMediatorEndpoints(
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
                m_mediatorUdpEndpoint = nx::network::url::getEndpoint(udpUrl);
                m_mediatorUrl = tcpUrl;
            }
            else
            {
                NX_DEBUG(this, lm("Cannot fetch mediator address. HTTP %1")
                    .arg((int)resultCode));
            }

            for (auto& handler: m_fetchMediatorEndpointsHandlers)
                nx::utils::swapAndCall(handler, resultCode);
            m_fetchMediatorEndpointsHandlers.clear();
        });
}

void MediatorConnector::initializeUrlFetcher()
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

void MediatorConnector::establishTcpConnectionToMediatorAsync()
{
    auto createStunTunnelUrl =
        nx::network::url::Builder(*m_mediatorUrl)
            .appendPath(api::kStunOverHttpTunnelPath).toUrl();

    m_stunClient->connect(
        createStunTunnelUrl,
        [this, createStunTunnelUrl](SystemError::ErrorCode code)
        {
            if (code == SystemError::noError)
            {
                m_fetchEndpointRetryTimer->reset();
                // TODO: #ak m_stunClient is expected to invoke "reconnected" handler here.
            }
            else
            {
                NX_DEBUG(this, lm("Failed to connect to mediator on %1. %2")
                    .args(createStunTunnelUrl, SystemError::toString(code)));
                reconnectToMediator();
            }

            if (!isReady(*m_future))
                m_promise->set_value(code == SystemError::noError);
        });
}

void MediatorConnector::reconnectToMediator()
{
    NX_DEBUG(this, lm("Connection to mediator has been broken. Reconnecting..."));

    m_fetchEndpointRetryTimer->scheduleNextTry(
        [this]()
        {
            if (m_mockedUpMediatorUrl)
            {
                NX_DEBUG(this, lm("Using mocked up mediator URL %1")
                    .args(*m_mockedUpMediatorUrl));
                establishTcpConnectionToMediatorAsync();
            }
            else
            {
                // Fetching mediator URL again.
                connectToMediatorAsync();
            }
        });
}

} // namespace api
} // namespace hpm
} // namespace nx
