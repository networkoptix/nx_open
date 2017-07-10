#include "mediator_connector.h"

#include <nx/network/socket_factory.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

static const std::chrono::milliseconds kRetryIntervalInitial = std::chrono::seconds(1);
static const std::chrono::milliseconds kRetryIntervalMax = std::chrono::minutes(10);

namespace nx {
namespace hpm {
namespace api {

namespace {

static stun::AbstractAsyncClient::Settings s_stunClientSettings;

} // namespace

MediatorConnector::MediatorConnector():
    m_stunClient(std::make_shared<stun::AsyncClientWithHttpTunneling>(s_stunClientSettings)),
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

void MediatorConnector::enable( bool waitComplete )
{
    bool needToFetch = false;
    {
        QnMutexLocker lk( &m_mutex );
        if( !m_promise )
        {
            needToFetch = true;
            m_promise = nx::utils::promise< bool >();
            m_future = m_promise->get_future();
        }
    }

    if( needToFetch )
        fetchEndpoint();

    if( waitComplete )
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

void MediatorConnector::mockupAddress(QUrl mediatorUrl, bool suppressWarning )
{
    {
        QnMutexLocker lk( &m_mutex );
        if (m_promise && (mediatorUrl == m_mediatorUrl))
            return;

        NX_ASSERT( !m_promise, Q_FUNC_INFO,
                    "Address resolving is already in progress!" );

        m_promise = nx::utils::promise< bool >();
        m_future = m_promise->get_future();
    }

    if (!suppressWarning)
    {
        NX_DEBUG(this, lm("Mediator address is mocked up: %1").arg(mediatorUrl));
    }

    m_mediatorUrl = mediatorUrl;
    m_mediatorUdpEndpoint = nx::network::url::getEndpoint(mediatorUrl);
    m_stunClient->connect(nx::network::url::getEndpoint(mediatorUrl));
    m_promise->set_value( true );
}

void MediatorConnector::setSystemCredentials( boost::optional<SystemCredentials> value )
{
    bool needToReconnect = false;
    {
        QnMutexLocker lk( &m_mutex );
        if( m_credentials == value )
            return;

        needToReconnect = static_cast<bool>( m_credentials );
        m_credentials = std::move(value);
    }

    if (needToReconnect)
        m_stunClient->closeConnection( SystemError::connectionReset );
}

boost::optional<SystemCredentials> MediatorConnector::getSystemCredentials() const
{
    QnMutexLocker lk( &m_mutex );
    return m_credentials;
}

boost::optional<SocketAddress> MediatorConnector::udpEndpoint() const
{
    QnMutexLocker lk(&m_mutex);
    return m_mediatorUdpEndpoint;
}

bool MediatorConnector::isConnected() const
{
    QnMutexLocker lk(&m_mutex);
    return static_cast<bool>(m_mediatorUrl);
}

void MediatorConnector::setStunClientSettings(
    stun::AbstractAsyncClient::Settings stunClientSettings)
{
    s_stunClientSettings = std::move(stunClientSettings);
}

static bool isReady(nx::utils::future<bool> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void MediatorConnector::fetchEndpoint()
{
    m_mediatorUrlFetcher->get(
        [ this ](nx_http::StatusCode::Value status, QUrl tcpUrl, QUrl udpUrl)
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                     .arg( status ), cl_logDEBUG1 );

            if (!isReady(*m_future))
                m_promise->set_value( false );

            // retry after some delay
            m_fetchEndpointRetryTimer->scheduleNextTry([this]() { fetchEndpoint(); });
        }
        else
        {
            NX_DEBUG(this, lm("Fetched mediator tcp (%1) and udp (%2) urls")
                .arg(tcpUrl).arg(udpUrl));
            m_mediatorUdpEndpoint = nx::network::url::getEndpoint(udpUrl);
            useMediatorUrl(tcpUrl);
        }
    });
}

void MediatorConnector::useMediatorUrl(QUrl url)
{
    m_stunClient->connect(
        SocketAddress(url.host(), url.port()),
        false /* SSL disabled */,
        [this](SystemError::ErrorCode code)
        {
            auto setEndpoint =
                [this]()
            {
                QnMutexLocker lk(&m_mutex);
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

void MediatorConnector::stopWhileInAioThread()
{
    m_stunClient.reset();
    m_mediatorUrlFetcher.reset();
    m_fetchEndpointRetryTimer.reset();
}

} // namespace api
} // namespace hpm
} // namespace nx
