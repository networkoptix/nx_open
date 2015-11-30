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
    , m_endpointFetcher(
        lit( "hpm" ),
        std::make_unique<RandomEndpointSelector>() )
    , m_timerSocket( SocketFactory::createStreamSocket() )
{
}

MediatorConnector::~MediatorConnector()
{
    {
        QnMutexLocker lk( &m_mutex );
        m_isTerminating = true;
    }

    m_timerSocket->terminateAsyncIO( true );
}

void MediatorConnector::enable( bool waitComplete )
{
    bool needToFetch = false;
    {
        QnMutexLocker lk( &m_mutex );
        if( !m_promise && !m_endpoint )
        {
            needToFetch = true;
            m_promise = std::promise< bool >();
        }
    }

    if( needToFetch )
        m_endpointFetcher.get(
            [ this ]( nx_http::StatusCode::Value status, SocketAddress address )
        {
            if( status != nx_http::StatusCode::ok )
            {
                NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                         .arg( status ), cl_logERROR );

                QnMutexLocker lk( &m_mutex );
                m_promise->set_value( false );

                // retry after some delay
                if( !m_isTerminating )
                    m_timerSocket->registerTimer( RETRY_INTERVAL.count(),
                                                  [ this ](){ enable( false ); } );
            }
            else
            {
                NX_LOGX( lit( "Fetched mediator address: %1" )
                         .arg( address.toString() ), cl_logALWAYS );

                QnMutexLocker lk( &m_mutex );
                if ( !m_endpoint )
                    m_endpoint = std::move( address );
                m_promise->set_value( true );
            }
        });

    if( waitComplete )
    {
        QnMutexLocker lk( &m_mutex );
        if( m_promise )
            m_promise->get_future().wait();
    }
}

stun::AsyncClient* MediatorConnector::client()
{
    QnMutexLocker lk( &m_mutex );
    if( !m_client && m_endpoint )
    {
        m_client = std::make_unique< stun::AsyncClient >( *m_endpoint );
        m_client->openConnection(
            [ this ]( SystemError::ErrorCode code )
            {
                if( code == SystemError::noError )
                {
                    NX_LOGX( lit( "Mediator connection is estabilished" ),
                             cl_logDEBUG1 );
                }
                else
                {
                    NX_LOGX( lit( "Mediator connection has failed: %1" )
                             .arg( SystemError::toString( code ) ),
                             cl_logDEBUG1 );
                }

            },
            [ this ]( SystemError::ErrorCode code )
            {
                NX_LOGX( lit( "Mediator connection has been lost: %1" )
                         .arg( SystemError::toString( code ) ),
                         cl_logDEBUG1 );
            } );
    }

    return m_client.get();
}

void MediatorConnector::mockupAddress( SocketAddress address )
{
    QnMutexLocker lk( &m_mutex );
    Q_ASSERT_X( !m_endpoint, Q_FUNC_INFO, "Address is already resolved!" );

    m_endpoint = std::move( address );
    NX_LOGX( lit( "Mediator address is mocked up: %1" )
             .arg( m_endpoint->toString() ), cl_logWARNING );
}

}   //cc
}   //nx
