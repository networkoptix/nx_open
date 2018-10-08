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
    m_mediatorEndpointProvider(
        std::make_unique<MediatorEndpointProvider>(cloudHost)),
    m_fetchEndpointRetryTimer(
        std::make_unique<nx::network::RetryTimer>(
            s_stunClientSettings.reconnectPolicy))
{
    // NOTE: Reconnect to mediator is handled by this class, not by STUN client.

    auto stunClientSettings = s_stunClientSettings;
    stunClientSettings.reconnectPolicy = network::RetryPolicy::kNoRetries;
    m_stunClient = std::make_shared<DelayedConnectStunClient>(
        m_mediatorEndpointProvider.get(),
        std::make_unique<network::stun::AsyncClientWithHttpTunneling>(
            stunClientSettings));

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
    if (m_mediatorEndpointProvider)
        m_mediatorEndpointProvider->bindToAioThread(aioThread);
    m_fetchEndpointRetryTimer->bindToAioThread(aioThread);
}

void MediatorConnector::enable(bool waitForCompletion)
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

    if (waitForCompletion)
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
    m_mediatorEndpointProvider->mockupCloudModulesXmlUrl(cloudModulesXmlUrl);
}

void MediatorConnector::mockupMediatorUrl(
    const nx::utils::Url& mediatorUrl,
    const network::SocketAddress stunUdpEndpoint)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (const auto currentMediatorUrl = m_mediatorEndpointProvider->mediatorUrl();
            m_promise && currentMediatorUrl && mediatorUrl == *currentMediatorUrl)
        {
            return;
        }

        NX_ASSERT(!m_promise, Q_FUNC_INFO,
            "Address resolving is already in progress!");

        m_promise = nx::utils::promise<bool>();
        m_future = m_promise->get_future();

        NX_DEBUG(this, lm("Mediator address is mocked up: %1").arg(mediatorUrl));

        m_mockedUpMediatorUrl = mediatorUrl;
        m_mediatorEndpointProvider->mockupMediatorUrl(
            mediatorUrl,
            stunUdpEndpoint);
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
            if (const auto endpoint = m_mediatorEndpointProvider->udpEndpoint())
                return handler(network::http::StatusCode::ok, *endpoint);

            m_mediatorEndpointProvider->fetchMediatorEndpoints(
                [this, handler = std::move(handler)](
                    nx::network::http::StatusCode::Value resultCode)
                {
                    if (nx::network::http::StatusCode::isSuccessCode(resultCode))
                        handler(resultCode, *m_mediatorEndpointProvider->udpEndpoint());
                    else
                        handler(resultCode, nx::network::SocketAddress());
                });
        });
}

std::optional<nx::network::SocketAddress> MediatorConnector::udpEndpoint() const
{
    return m_mediatorEndpointProvider->udpEndpoint();
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
    if (m_mediatorEndpointProvider)
        m_mediatorEndpointProvider->pleaseStopSync();
    m_fetchEndpointRetryTimer.reset();
}

void MediatorConnector::connectToMediatorAsync()
{
    m_mediatorEndpointProvider->fetchMediatorEndpoints(
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

void MediatorConnector::establishTcpConnectionToMediatorAsync()
{
    NX_ASSERT(m_mediatorEndpointProvider->mediatorUrl());

    auto createStunTunnelUrl =
        nx::network::url::Builder(*m_mediatorEndpointProvider->mediatorUrl())
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

//-------------------------------------------------------------------------------------------------

DelayedConnectStunClient::DelayedConnectStunClient(
    MediatorEndpointProvider* endpointProvider,
    std::unique_ptr<nx::network::stun::AbstractAsyncClient> stunClient)
    :
    base_type(std::move(stunClient)),
    m_endpointProvider(endpointProvider)
{
}

void DelayedConnectStunClient::connect(
    const nx::utils::Url& url,
    ConnectHandler handler)
{
    dispatch(
        [this, url, handler = std::move(handler)]() mutable
        {
            m_urlKnown = true;
            base_type::connect(url, std::move(handler));
        });
}

void DelayedConnectStunClient::sendRequest(
    nx::network::stun::Message request,
    RequestHandler handler,
    void* client)
{
    dispatch(
        [this, request = std::move(request),
            handler = std::move(handler), client]() mutable
        {
            if (m_urlKnown)
            {
                base_type::sendRequest(std::move(request), std::move(handler), client);
                return;
            }

            NX_ASSERT(m_endpointProvider->getAioThread() == getAioThread());

            m_endpointProvider->fetchMediatorEndpoints(
                [this](auto... args) { onFetchEndpointCompletion(std::move(args)...); });

            m_postponedRequests.push_back(
                {std::move(request), std::move(handler), client});
        });
}

void DelayedConnectStunClient::onFetchEndpointCompletion(
    nx::network::http::StatusCode::Value resultCode)
{
    if (!nx::network::http::StatusCode::isSuccessCode(resultCode))
        return failPendingRequests(SystemError::hostUnreachable);

    m_urlKnown = true;

    const auto createStunTunnelUrl =
        nx::network::url::Builder(*m_endpointProvider->mediatorUrl())
            .appendPath(api::kStunOverHttpTunnelPath).toUrl();

    base_type::connect(createStunTunnelUrl, [](auto... /*args*/) {});

    sendPendingRequests();
}

void DelayedConnectStunClient::failPendingRequests(
    SystemError::ErrorCode resultCode)
{
    auto postponedRequests = std::exchange(m_postponedRequests, {});
    for (auto& requestContext: postponedRequests)
        requestContext.handler(resultCode, nx::network::stun::Message());
}

void DelayedConnectStunClient::sendPendingRequests()
{
    auto postponedRequests = std::exchange(m_postponedRequests, {});
    for (auto& requestContext: postponedRequests)
    {
        base_type::sendRequest(
            std::move(requestContext.request),
            std::move(requestContext.handler),
            requestContext.client);
    }
}

//-------------------------------------------------------------------------------------------------

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

void MediatorEndpointProvider::mockupMediatorUrl(
    const nx::utils::Url& mediatorUrl,
    const network::SocketAddress stunUdpEndpoint)
{
    QnMutexLocker lock(&m_mutex);

    m_mediatorUrl = mediatorUrl;
    m_mediatorUdpEndpoint = stunUdpEndpoint;
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

std::optional<nx::utils::Url> MediatorEndpointProvider::mediatorUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mediatorUrl;
}

std::optional<network::SocketAddress> MediatorEndpointProvider::udpEndpoint() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mediatorUdpEndpoint;
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

} // namespace api
} // namespace hpm
} // namespace nx
