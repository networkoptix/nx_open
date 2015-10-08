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
    // TODO #mu: update values over time and deliver changes
    m_mediatorEndpointFetcher.get( [ = ]( nx_http::StatusCode::Value status,
                                          SocketAddress address )
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOGX( lit( "Can not fetch mediator address: HTTP %1" )
                     .arg( status ), cl_logERROR );
            return;
        }

        NX_LOGX( lit( "Fetched mediator address: %1" )
                 .arg( address.toString() ), cl_logALWAYS );

        QnMutexLocker lk( &m_mutex );
        m_mediatorAddress = std::move( address );
    });
}

boost::optional< SocketAddress > CloudConnectionInfo::mediatorAddress() const
{
    QnMutexLocker lk( &m_mutex );
    return m_mediatorAddress;
}

}   //cc
}   //nx
