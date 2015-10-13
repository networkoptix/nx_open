#include "cloud_connection_info.h"

#include "common/common_globals.h"
#include "utils/common/cpp14.h"
#include "utils/common/log.h"

static const std::chrono::minutes RETRY_INTERVAL( 10 );

namespace nx {
namespace cc {

CloudConnectionInfo::CloudConnectionInfo()
    : m_isTerminating( false )
    , m_mediatorEndpointFetcher(
        lit( "hpm" ),
        std::make_unique<RandomEndpointSelector>() )
{
}

CloudConnectionInfo::~CloudConnectionInfo()
{
    QnMutexLocker lk( &m_mutex );
    m_isTerminating = true;
}

void CloudConnectionInfo::enableMediator( bool waitComplete )
{
    bool needToFetch = false;
    {
        QnMutexLocker lk( &m_mutex );
        if( !m_mediatorPromise )
        {
            needToFetch = true;
            m_mediatorPromise = std::promise< bool >();
        }
    }

    if( needToFetch )
        m_mediatorEndpointFetcher.get(
            [ this ]( nx_http::StatusCode::Value status, SocketAddress address )
        {
            if( status != nx_http::StatusCode::ok )
            {
                NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                         .arg( status ), cl_logERROR );

                QnMutexLocker lk( &m_mutex );
                m_mediatorPromise->set_value( false );

                // retry after some delay
                if( !m_isTerminating )
                    m_timerGuard = TimerManager::instance()->addTimer(
                        [ this ]( quint64 ) { enableMediator( false ); },
                        RETRY_INTERVAL );
            }
            else
            {
                NX_LOGX( lit( "Fetched mediator address: %1" )
                         .arg( address.toString() ), cl_logALWAYS );

                QnMutexLocker lk( &m_mutex );
                m_mediatorAddress = std::move( address );
                m_mediatorPromise->set_value( true );
            }
        });

    if( waitComplete )
        m_mediatorPromise->get_future().wait();
}

boost::optional< SocketAddress > CloudConnectionInfo::mediatorAddress() const
{
    QnMutexLocker lk( &m_mutex );
    return m_mediatorAddress;
}

}   //cc
}   //nx
