#include "cloud_connection_info.h"

#include "common/common_globals.h"
#include "utils/common/log.h"

namespace nx {
namespace cc {

CloudConnectionInfo::CloudConnectionInfo()
    : m_mediatorEndpointFetcher( lit( "hpm" ) )
{
}

void CloudConnectionInfo::enableMediator()
{
    m_mediatorEndpointFetcher.get( [ = ]( nx_http::StatusCode::Value status,
                                          SocketAddress address )
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOG( lit( "%1 Can not fetch mediator address: HTTP %2" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( status ),
                    cl_logERROR );
            return;
        }

        NX_LOG( lit( "%1 Fetched mediator address: %2" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                .arg( address.toString() ), cl_logALWAYS );

        QnMutexLocker lk( &m_mutex );
        m_mediatorAddress = std::move( address );
    });
}

boost::optional< SocketAddress > CloudConnectionInfo::mediatorAddress()
{
    QnMutexLocker lk( &m_mutex );
    return m_mediatorAddress;
}

}   //cc
}   //nx
