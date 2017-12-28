#include "mediator_connector.h"

#include <nx/network/socket_factory.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "connection_mediator_url_fetcher.h"

static const std::chrono::milliseconds kRetryIntervalInitial = std::chrono::seconds(1);
static const std::chrono::milliseconds kRetryIntervalMax = std::chrono::minutes(10);

namespace nx {
namespace hpm {
namespace api {

namespace {

static network::stun::AbstractAsyncClient::Settings s_stunClientSettings;

} // namespace

MediatorConnector::MediatorConnector():
    m_stunClient(std::make_shared<network::stun::AsyncClientWithHttpTunneling>(s_stunClientSettings)),
    m_mediatorUrlFetcher(std::make_unique<nx::network::cloud::ConnectionMediatorUrlFetcher>()),
    m_fetchEndpointRetryTimer(
        std::make_unique<nx::network::RetryTimer>(
            nx::network::RetryPolicy(
                nx::network::RetryPolicy::kInfiniteRetries,
                kRetryIntervalInitial,
                2,
                kRetryIntervalMax)))
{
    bindToAioThread(getAioThread());

    NX_ASSERT(
        s_stunClientSettings.reconnectPolicy.maxRetryCount ==
        network::RetryPolicy::kInfiniteRetries);
}

MediatorConnector::~MediatorConnector()
{
    NX_ASSERT((m_stunClient == nullptr) || m_stunClient.unique());
    pleaseStopSync(false);
}

void MediatorConnector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_stunClient->bindToAioThread(aioThread);
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
        fetchEndpoint();

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

void MediatorConnector::mockupCloudModulesXmlUrl(const nx::utils::Url& cloudModulesXmlUrl)
{
    QnMutexLocker lock(&m_mutex);
    m_mediatorUrlFetcher->setModulesXmlUrl(cloudModulesXmlUrl);
}

void MediatorConnector::mockupMediatorUrl(const nx::utils::Url& mediatorUrl)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_promise && mediatorUrl == m_mediatorUrl)
            return;

        NX_ASSERT(!m_promise, Q_FUNC_INFO,
            "Address resolving is already in progress!");

        m_promise = nx::utils::promise<bool>();
        m_future = m_promise->get_future();
    }

    NX_DEBUG(this, lm("Mediator address is mocked up: %1").arg(mediatorUrl));

    m_mediatorUrl = mediatorUrl;
    m_mediatorUdpEndpoint = nx::network::url::getEndpoint(mediatorUrl);
    m_stunClient->connect(mediatorUrl, [](SystemError::ErrorCode) {});
    m_promise->set_value(true);

    if (m_mediatorAvailabilityChangedHandler)
        m_mediatorAvailabilityChangedHandler(true);
}

void MediatorConnector::setSystemCredentials(boost::optional<SystemCredentials> value)
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

boost::optional<SystemCredentials> MediatorConnector::getSystemCredentials() const
{
    QnMutexLocker lock(&m_mutex);
    return m_credentials;
}

boost::optional<network::SocketAddress> MediatorConnector::udpEndpoint() const
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

void MediatorConnector::fetchEndpoint()
{
    m_mediatorUrlFetcher->get(
        [this](nx::network::http::StatusCode::Value status, nx::utils::Url tcpUrl, nx::utils::Url udpUrl)
        {
            if (status != nx::network::http::StatusCode::ok)
            {
                NX_LOGX(lit("Can not fetch mediator address: HTTP %1")
                    .arg(status), cl_logDEBUG1);

                if (!isReady(*m_future))
                    m_promise->set_value(false);

                // Retry after some delay.
                m_fetchEndpointRetryTimer->scheduleNextTry([this]() { fetchEndpoint(); });
            }
            else
            {
                NX_DEBUG(this, lm("Fetched mediator tcp (%1) and udp (%2) urls")
                    .arg(tcpUrl).arg(udpUrl));
                m_mediatorUdpEndpoint = nx::network::url::getEndpoint(udpUrl);
                m_mediatorUrl = tcpUrl;
                connectToMediatorAsync();
                if (m_mediatorAvailabilityChangedHandler)
                    m_mediatorAvailabilityChangedHandler(true);
            }
        });
}

void MediatorConnector::connectToMediatorAsync()
{
    m_stunClient->connect(
        *m_mediatorUrl,
        [this](SystemError::ErrorCode code)
        {
            auto setEndpoint =
                [this]()
                {
                    QnMutexLocker lock(&m_mutex);
                    // NOTE: Assuming that mediator's UDP and TCP interfaces are available on the same IP.
                    m_mediatorUdpEndpoint->address = m_stunClient->remoteAddress().address;
                    NX_DEBUG(this, lm("Connected to mediator at %1").arg(m_mediatorUrl));
                };

            if (code == SystemError::noError)
                setEndpoint();

            if (!isReady(*m_future))
                m_promise->set_value(code == SystemError::noError);

            m_stunClient->addOnReconnectedHandler(std::move(setEndpoint));
        });
}

} // namespace api
} // namespace hpm
} // namespace nx
