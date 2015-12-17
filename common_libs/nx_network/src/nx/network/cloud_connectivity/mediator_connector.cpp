#include "mediator_connector.h"

#include "common/common_globals.h"
#include "utils/common/cpp14.h"
#include <nx/utils/log/log.h>
#include <nx/network/socket_factory.h>

static const std::chrono::milliseconds RETRY_INTERVAL = std::chrono::minutes( 10 );

namespace nx {
namespace cc {

MediatorConnector::MediatorConnector()
    : m_isTerminating( false )
    , m_stunClient(std::make_shared<stun::AsyncClient>())
    , m_endpointFetcher(
        lit( "hpm" ),
        std::make_unique<RandomEndpointSelector>() )
    , m_timerSocket( SocketFactory::createStreamSocket() )
{
}

void MediatorConnector::enable( bool waitComplete )
{
    bool needToFetch = false;
    {
        QnMutexLocker lk( &m_mutex );
        if( !m_promise )
        {
            needToFetch = true;
            m_promise = std::promise< bool >();
        }
    }

    if( needToFetch )
        fetchEndpoint();

    if( waitComplete )
        m_promise->get_future().wait();
}

std::shared_ptr<MediatorClientConnection> MediatorConnector::clientConnection()
{
    return std::shared_ptr<MediatorClientConnection>(
                new MediatorClientConnection( m_stunClient ) );
}

std::shared_ptr<MediatorSystemConnection> MediatorConnector::systemConnection()
{
    return std::shared_ptr<MediatorSystemConnection>(
                new MediatorSystemConnection( m_stunClient, this ) );
}

void MediatorConnector::mockupAddress( SocketAddress address )
{
    {
        QnMutexLocker lk( &m_mutex );
        Q_ASSERT_X( !m_promise, Q_FUNC_INFO,
                    "Address resolving is already in progress!" );

        m_promise = std::promise< bool >();
    }


    NX_LOGX( lit( "Mediator address is mocked up: %1" )
             .arg( address.toString() ), cl_logWARNING );

    m_stunClient->connect( address );
    m_promise->set_value( true );
}

bool MediatorConnector::SystemCredentials::operator ==( const SystemCredentials& rhs ) const
{
    return serverId == rhs.serverId &&
           systemId == rhs.systemId &&
           key      == rhs.key;
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

boost::optional<MediatorConnector::SystemCredentials>
    MediatorConnector::getSystemCredentials()
{
    QnMutexLocker lk( &m_mutex );
    return m_credentials;
}

void MediatorConnector::pleaseStop( std::function<void()> handler )
{
    {
        QnMutexLocker lk( &m_mutex );
        m_isTerminating = true;
    }

    m_timerSocket->pleaseStop(std::move(handler));
}

void MediatorConnector::fetchEndpoint()
{
    m_endpointFetcher.get(
        [ this ]( nx_http::StatusCode::Value status, SocketAddress address )
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                     .arg( status ), cl_logERROR );

            m_promise->set_value( false );

            // retry after some delay
            if( !m_isTerminating )
                m_timerSocket->registerTimer( RETRY_INTERVAL.count(),
                                              [ this ](){ fetchEndpoint(); } );
        }
        else
        {
            NX_LOGX( lit( "Fetched mediator address: %1" )
                     .arg( address.toString() ), cl_logALWAYS );


            m_stunClient->connect( address );
            m_promise->set_value( true );
        }
    });
}

}   //cc
}   //nx
