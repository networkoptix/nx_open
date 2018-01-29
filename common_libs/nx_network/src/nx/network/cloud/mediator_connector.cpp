#include "mediator_connector.h"

#include <nx/network/socket_factory.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {
namespace api {

namespace { static stun::AbstractAsyncClient::Settings s_stunClientSettings; }

MediatorConnector::MediatorConnector():
    m_fetchEndpointRetryTimer(
        std::make_unique<nx::network::RetryTimer>(
            s_stunClientSettings.reconnectPolicy))
{
    // Reconnect to mediator is handled by this class, not by STUN client.
    auto stunClientSettings = s_stunClientSettings;
    stunClientSettings.reconnectPolicy = network::RetryPolicy::kNoRetries;
    m_stunClient = std::make_shared<stun::AsyncClientWithHttpTunneling>(
        s_stunClientSettings);

    bindToAioThread(getAioThread());

    m_stunClient->setOnConnectionClosedHandler(
        std::bind(&MediatorConnector::reconnectToMediator, this));
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

void MediatorConnector::mockupCloudModulesXmlUrl(const QUrl& cloudModulesXmlUrl)
{
    m_cloudModulesXmlUrl = cloudModulesXmlUrl;
}

void MediatorConnector::mockupMediatorUrl(const QUrl& mediatorUrl)
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
    m_mockedUpMediatorUrl = mediatorUrl;
    m_mediatorUdpEndpoint = nx::network::url::getEndpoint(mediatorUrl);
    m_stunClient->connect(mediatorUrl, [](SystemError::ErrorCode) {});
    m_promise->set_value(true);
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

boost::optional<SocketAddress> MediatorConnector::udpEndpoint() const
{
    QnMutexLocker lock(&m_mutex);
    return m_mediatorUdpEndpoint;
}

bool MediatorConnector::isConnected() const
{
    QnMutexLocker lock(&m_mutex);
    return static_cast<bool>(m_mediatorUrl);
}

void MediatorConnector::setStunClientSettings(
    stun::AbstractAsyncClient::Settings stunClientSettings)
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
    if (!m_mediatorUrlFetcher)
    {
        m_mediatorUrlFetcher =
            std::make_unique<nx::network::cloud::ConnectionMediatorUrlFetcher>();
        m_mediatorUrlFetcher->bindToAioThread(getAioThread());
        if (m_cloudModulesXmlUrl)
            m_mediatorUrlFetcher->setModulesXmlUrl(*m_cloudModulesXmlUrl);
    }

    m_mediatorUrlFetcher->get(
        [this](nx_http::StatusCode::Value status, QUrl tcpUrl, QUrl udpUrl)
        {
            m_mediatorUrlFetcher.reset();

            if (status != nx_http::StatusCode::ok)
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
            }
        });
}

void MediatorConnector::connectToMediatorAsync()
{
    m_stunClient->connect(
        *m_mediatorUrl,
        [this](SystemError::ErrorCode code)
        {
            if (code == SystemError::noError)
            {
                m_fetchEndpointRetryTimer->reset();
                saveMediatorEndpoint();
                // TODO: ak m_stunClient is expected to invoke "reconnected" handler here.
            }
            else
            {
                NX_DEBUG(this, lm("Failed to connect to mediator on %1. %2")
                    .args(*m_mediatorUrl, SystemError::toString(code)));
                reconnectToMediator();
            }

            if (!isReady(*m_future))
                m_promise->set_value(code == SystemError::noError);
        });
}

void MediatorConnector::saveMediatorEndpoint()
{
    QnMutexLocker lock(&m_mutex);
    // NOTE: Assuming that mediator's UDP and TCP interfaces are available on the same IP.
    m_mediatorUdpEndpoint->address = m_stunClient->remoteAddress().address;
    NX_DEBUG(this, lm("Connected to mediator at %1").arg(m_mediatorUrl));
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
                connectToMediatorAsync();
            }
            else
            {
                // Fetching mediator URL again.
                fetchEndpoint();
            }
        });
}

} // namespace api
} // namespace hpm
} // namespace nx
