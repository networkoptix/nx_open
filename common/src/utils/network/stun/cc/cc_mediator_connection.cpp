#include "cc_mediator_connection.h"

#include "custom_stun.h"

#include "common/common_globals.h"
#include "utils/common/log.h"

namespace nx {
namespace stun {
namespace cc {

MediatorConnection::MediatorConnection( const SocketAddress& address,
                                        const String& hostName,
                                        const String& authorizationKey )
    : m_stunClient( address )
    , m_hostName( hostName )
    , m_authorizationKey( authorizationKey )
    , m_isExternalChanged( false )
    , m_isExternalUpdateInProgress( false )
{
}

void MediatorConnection::updateAddresses( const std::list< SocketAddress >& addresses )
{
    std::set< SocketAddress > current( addresses.begin(), addresses.end() );

    QnMutexLocker lk( &m_mutex );
    for( auto it = m_external.begin(); it != m_external.end(); )
    {
        const auto curIt = current.find( *it );
        if( curIt != current.end() )
        {
            // no need to ping this one
            current.erase( curIt );
            ++it;
        }
        else
        {
            m_external.erase( it );
            m_isExternalChanged = true;
        }
    }

    updateExternalAddresses();
    checkAddresses( std::list< SocketAddress >( current.begin(), current.end() ) );
}

void MediatorConnection::updateExternalAddresses()
{
    if( !m_isExternalChanged || m_isExternalUpdateInProgress )
        return;

    Message request( Header( MessageClass::request, cc::methods::bind ) );
    request.newAttribute< cc::attrs::HostName >( m_hostName );
    request.newAttribute< cc::attrs::Authorization >( m_authorizationKey );
    request.newAttribute< cc::attrs::PublicEndpointList >(
        std::list< SocketAddress >( m_external.begin(), m_external.end() ) );

    const auto ret = m_stunClient.sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, Message response )
    {
        QnMutexLocker lk( &m_mutex );
        m_isExternalUpdateInProgress = false;

        if( const auto error = stun::AsyncClient::hasError( code, response ) )
        {
            NX_LOG( lit( "%1 %2" ).arg( QString::fromUtf8( Q_FUNC_INFO ) )
                                  .arg( *error ), cl_logERROR );
            m_isExternalChanged = true;
        }

        // just in case if smthing already changed
        updateExternalAddresses();
    } );

    if( ret )
    {
        m_isExternalChanged = false;
        m_isExternalUpdateInProgress = true;
    }
}

void MediatorConnection::checkAddresses( const std::list< SocketAddress >& addresses )
{
    Message request( Header( MessageClass::request, cc::methods::ping ) );
    request.newAttribute< cc::attrs::HostName >( m_hostName );
    request.newAttribute< cc::attrs::Authorization >( m_authorizationKey );
    request.newAttribute< cc::attrs::PublicEndpointList >( addresses );

    const auto ret = m_stunClient.sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, Message response )
    {
        if( const auto error = stun::AsyncClient::hasError( code, response ) )
        {
            NX_LOG( lit( "%1 %2" ).arg( QString::fromUtf8( Q_FUNC_INFO ) )
                                  .arg( *error ), cl_logERROR );
            return;
        }

        const auto eps = response.getAttribute< cc::attrs::PublicEndpointList >();
        if( !eps )
        {
            NX_LOG( lit( "%1 No PublicEndpointList in response" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) ), cl_logERROR );
            return;
        }

        QnMutexLocker lk( &m_mutex );
        const auto size = m_external.size();
        const auto addresses = eps->get();
        m_external.insert( addresses.begin(), addresses.end() );

        m_isExternalChanged = (size != m_external.size());
        updateExternalAddresses();
    } );
}

} // namespase cc
} // namespase stun
} // namespase nx
