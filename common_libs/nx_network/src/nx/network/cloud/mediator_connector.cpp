
#include "mediator_connector.h"

#include "common/common_globals.h"
#include "nx/utils/std/cpp14.h"

#include <nx/utils/log/log.h>
#include <nx/network/socket_factory.h>


static const std::chrono::milliseconds kRetryIntervalInitial = std::chrono::seconds(1);
static const std::chrono::milliseconds kRetryIntervalMax = std::chrono::minutes( 10 );

namespace nx {
namespace hpm {
namespace api {

MediatorConnector::MediatorConnector()
:
    m_isTerminating( false ),
    m_stunClient(std::make_shared<stun::AsyncClient>()),
    m_endpointFetcher(
        lit( "hpm" ),
        std::make_unique<nx::network::cloud::RandomEndpointSelector>() ),
    m_fetchEndpointRetryTimer(
        nx::network::RetryPolicy(
            nx::network::RetryPolicy::kInfiniteRetries,
            kRetryIntervalInitial,
            2,
            kRetryIntervalMax))
{
}

void MediatorConnector::reinitializeStunClient(
    stun::AbstractAsyncClient::Settings stunClientSettings)
{
    m_stunClient = std::make_shared<stun::AsyncClient>(stunClientSettings);
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

void MediatorConnector::mockupAddress( SocketAddress address, bool suppressWarning )
{
    {
        QnMutexLocker lk( &m_mutex );
        if (m_promise && (address == m_mediatorAddress))
            return;

        NX_ASSERT( !m_promise, Q_FUNC_INFO,
                    "Address resolving is already in progress!" );

        m_promise = nx::utils::promise< bool >();
        m_future = m_promise->get_future();
    }

    if (!suppressWarning)
    {
        NX_LOGX( lit( "Mediator address is mocked up: %1" )
                 .arg( address.toString() ), cl_logWARNING );
    }

    m_mediatorAddress = address;
    m_stunClient->connect( std::move( address ) );
    m_promise->set_value( true );
}

void MediatorConnector::mockupAddress( const MediatorConnector& connector )
{
    if (auto address = connector.mediatorAddress())
        mockupAddress(std::move(*address), true);
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

void MediatorConnector::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    {
        QnMutexLocker lk( &m_mutex );
        m_isTerminating = true;
    }

    m_fetchEndpointRetryTimer.pleaseStop(std::move(handler));
}

boost::optional<SocketAddress> MediatorConnector::mediatorAddress() const
{
    QnMutexLocker lk(&m_mutex);
    return m_mediatorAddress;
}

static bool isReady(nx::utils::future<bool> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void MediatorConnector::fetchEndpoint()
{
    m_endpointFetcher.get(
        [ this ]( nx_http::StatusCode::Value status, SocketAddress address )
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                     .arg( status ), cl_logDEBUG1 );

            if (!isReady(*m_future))
                m_promise->set_value( false );

            // retry after some delay
            if (!m_isTerminating)
            {
                m_fetchEndpointRetryTimer.scheduleNextTry(
                    [this]() { fetchEndpoint(); });
            }
        }
        else
        {
            NX_LOGX( lit( "Fetched mediator address: %1" )
                     .arg( address.toString() ), cl_logDEBUG1 );

            {
                QnMutexLocker lk(&m_mutex);
                m_mediatorAddress = address;
            }
            m_stunClient->connect( std::move( address ) );
            if (!isReady(*m_future))
                m_promise->set_value( true );
        }
    });
}

} // namespace api
} // namespace hpm
} // namespace nx
