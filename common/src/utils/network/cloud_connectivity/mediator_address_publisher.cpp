#include "mediator_address_publisher.h"

#include "utils/network/stun/cc/custom_stun.h"

#include "common/common_globals.h"
#include "utils/common/log.h"

namespace nx {
namespace cc {

MediatorAddressPublisher::MediatorAddressPublisher(
        CloudModuleEndPointFetcher* addressFetcher, String serverId )
    : m_serverId( std::move( serverId ) )
{
    addressFetcher->get( [ this ]( nx_http::StatusCode::Value status,
                                   SocketAddress address )
    {
        if( status != nx_http::StatusCode::ok )
        {
            NX_LOG( lit( "%1 could not fetch mediator address: %2" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( status ), cl_logERROR );

            // TODO: retry later? when?
            return;
        }

        QnMutexLocker lk( &m_mutex );
        m_address = std::move( address );
        m_stunClient = std::make_unique< stun::AsyncClient >( *m_address );
        updateExternalAddresses();
    } );
}

bool MediatorAddressPublisher::Authorization::operator == ( const Authorization& rhs ) const
{
    return systemId == rhs.systemId && key == rhs.key;
}

void MediatorAddressPublisher::authorizationChanged(
        boost::optional< Authorization > authorization )
{
    std::unique_ptr< stun::AsyncClient > oldStunClient;
    {
        QnMutexLocker lk( &m_mutex );
        if( m_authorization == authorization )
            return;

        m_authorization = std::move( authorization );
        if( !m_address )
            return;

        std::swap( oldStunClient, m_stunClient );
    }

    oldStunClient.reset();

    {
        QnMutexLocker lk( &m_mutex );
        m_stunClient = std::make_unique< stun::AsyncClient >( *m_address );
        m_isExternalChanged = true;
        m_isExternalUpdateInProgress = false;
        updateExternalAddresses();
    }
}

void MediatorAddressPublisher::updateAddresses( std::list< SocketAddress > addresses )
{
    QnMutexLocker lk( &m_mutex );
    for( auto it = m_external.begin(); it != m_external.end(); )
    {
        const auto curIt = std::find(addresses.begin(), addresses.end(),  *it );
        if( curIt != addresses.end() )
        {
            // no need to ping this one
            addresses.erase( curIt );
            ++it;
        }
        else
        {
            m_external.erase( it );
            m_isExternalChanged = true;
        }
    }

    updateExternalAddresses();
    checkAddresses( std::move( addresses ) );
}

void MediatorAddressPublisher::updateExternalAddresses()
{
    if( !m_stunClient || !m_authorization )
        return;

    if( !m_isExternalChanged || m_isExternalUpdateInProgress )
        return;

    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::bind ) );
    request.newAttribute< stun::cc::attrs::SystemId >( m_authorization->systemId );
    request.newAttribute< stun::cc::attrs::ServerId >( m_serverId );
    request.newAttribute< stun::cc::attrs::Authorization >( m_authorization->key );
    request.newAttribute< stun::cc::attrs::PublicEndpointList >(
        std::list< SocketAddress >( m_external.begin(), m_external.end() ) );

    const auto ret = m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, stun::Message response )
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

    if( !ret )
    {
        NX_LOG( lit( "%1 Could not sendRequest" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) ), cl_logERROR );
        return;
    }

    m_isExternalChanged = false;
    m_isExternalUpdateInProgress = true;
}

void MediatorAddressPublisher::checkAddresses( const std::list< SocketAddress >& addresses )
{
    if( !m_stunClient || !m_authorization )
        return;

    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::ping ) );
    request.newAttribute< stun::cc::attrs::SystemId >( m_authorization->systemId );
    request.newAttribute< stun::cc::attrs::ServerId >( m_serverId );
    request.newAttribute< stun::cc::attrs::Authorization >( m_authorization->key );
    request.newAttribute< stun::cc::attrs::PublicEndpointList >( addresses );

    const auto ret = m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, stun::Message response )
    {
        if( const auto error = stun::AsyncClient::hasError( code, response ) )
        {
            NX_LOG( lit( "%1 %2" ).arg( QString::fromUtf8( Q_FUNC_INFO ) )
                                  .arg( *error ), cl_logERROR );
            return;
        }

        const auto eps = response.getAttribute< stun::cc::attrs::PublicEndpointList >();
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

    if (!ret )
        NX_LOG( lit( "%1 Could not sendRequest" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) ), cl_logERROR );
}

} // namespase cc
} // namespase nx
