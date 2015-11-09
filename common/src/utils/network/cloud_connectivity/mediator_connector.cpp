#include "mediator_connector.h"

#include "common/common_globals.h"
#include "utils/common/cpp14.h"
#include "utils/common/log.h"
#include "utils/network/socket_factory.h"

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
        if( !m_promise )
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
                m_endpoint = std::move( address );
                m_promise->set_value( true );
            }
        });

    if( waitComplete )
        m_promise->get_future().wait();
}

std::unique_ptr< stun::AsyncClient > MediatorConnector::client()
{
    QnMutexLocker lk( &m_mutex );
    if( !m_endpoint )
        return nullptr;

    return std::make_unique< stun::AsyncClient >( *m_endpoint );
}

void MediatorConnector::mockupAddress( SocketAddress address )
{
    QnMutexLocker lk( &m_mutex );
    m_endpoint = std::move( address );
}

}   //cc
}   //nx
