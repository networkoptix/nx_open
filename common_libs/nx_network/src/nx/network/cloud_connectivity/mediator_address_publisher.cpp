#include "mediator_address_publisher.h"

#include "common/common_globals.h"
#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/cc/custom_stun.h>

namespace nx {
namespace cc {

const TimerDuration MediatorAddressPublisher::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 10 );

MediatorAddressPublisher::MediatorAddressPublisher()
    : m_updateInterval( DEFAULT_UPDATE_INTERVAL )
    , isTerminating( false )
    , m_timerSocket( SocketFactory::createStreamSocket() )
    , m_stunClient( nullptr )
{
}

bool MediatorAddressPublisher::Authorization::operator == ( const Authorization& rhs ) const
{
    return serverId == rhs.serverId &&
           systemId == rhs.systemId &&
           key      == rhs.key;
}

void MediatorAddressPublisher::setUpdateInterval( TimerDuration updateInterval )
{
    QnMutexLocker lk( &m_mutex );
    m_updateInterval = updateInterval;
}

void MediatorAddressPublisher::updateAuthorization(
        boost::optional< Authorization > authorization )
{
    QnMutexLocker lk( &m_mutex );
    if( m_stunAuthorization == authorization )
        return;

    NX_LOGX( lm( "New credentials: %1" ).arg(
                 static_cast< bool >( authorization ) ?
                     authorization->systemId : String("none") ), cl_logDEBUG1 );

    m_stunAuthorization = std::move( authorization );
    if( m_stunAuthorization && !m_stunClient )
    {
        SocketGlobals::mediatorConnector().enable();
        pingReportedAddresses();
    }

    if( !m_stunAuthorization && m_stunClient )
    {
        lk.unlock();
        m_stunClient->closeConnection( SystemError::connectionReset );
    }
}

void MediatorAddressPublisher::updateAddresses( std::list< SocketAddress > addresses )
{
    QnMutexLocker lk( &m_mutex );
    if( m_reportedAddresses == addresses )
        return;

    NX_LOGX( lm( "New addresses: %1" ).container( addresses ), cl_logDEBUG1 );
    m_reportedAddresses = std::move( addresses );
}

void MediatorAddressPublisher::setupUpdateTimer()
{
    if( isTerminating )
        return;

    m_timerSocket->registerTimer( m_updateInterval.count(), [ this ]()
    {
        QnMutexLocker lk( &m_mutex );
        pingReportedAddresses();
    } );
}

bool MediatorAddressPublisher::isCloudReady()
{
    if( !m_stunClient )
    {
        m_stunClient = SocketGlobals::mediatorConnector().client();
        if( !m_stunClient )
        {
            NX_LOGX( lit( "Mediator address is not resolved yet" ), cl_logDEBUG1 );
            return false;
        }

        if( !m_stunAuthorization )
        {
            NX_LOGX( lit( "No authorization info yet" ), cl_logDEBUG1 );
            return false;
        }
    }

    return m_stunClient && m_stunAuthorization;
}

void MediatorAddressPublisher::pingReportedAddresses()
{
    if( !isCloudReady() )
        return setupUpdateTimer();

    if( m_reportedAddresses == m_pingedAddresses )
        return publishPingedAddresses();

    using namespace stun;
    using namespace stun::cc::attrs;

    Message request( Header( MessageClass::request, stun::cc::methods::ping ) );
    request.newAttribute< SystemId >( m_stunAuthorization->systemId );
    request.newAttribute< ServerId >( m_stunAuthorization->serverId );
    request.newAttribute< PublicEndpointList >( m_reportedAddresses );
    request.insertIntegrity( m_stunAuthorization->systemId, m_stunAuthorization->key );

    m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, Message response )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto error = AsyncClient::hasError( code, response ) )
        {
            NX_LOGX( *error, cl_logERROR );
            m_pingedAddresses.clear();
            m_publishedAddresses.clear();
            return setupUpdateTimer();
        }

        const auto eps = response.getAttribute< PublicEndpointList >();
        if( !eps )
        {
            NX_LOGX( lit( "No PublicEndpointList in response" ), cl_logERROR );
            m_pingedAddresses.clear();
            return setupUpdateTimer();
        }

        m_pingedAddresses = eps->get();
        NX_LOGX( lm( "Pinged addresses: %1" )
                 .container( m_publishedAddresses ), cl_logDEBUG1 );

        publishPingedAddresses();
    } );
}

void MediatorAddressPublisher::publishPingedAddresses()
{
    if( !isCloudReady() )
        return setupUpdateTimer();

    using namespace stun;
    using namespace stun::cc::attrs;

    Message request( Header( MessageClass::request, stun::cc::methods::bind ) );
    request.newAttribute< SystemId >( m_stunAuthorization->systemId );
    request.newAttribute< ServerId >( m_stunAuthorization->serverId );
    request.newAttribute< PublicEndpointList >( m_pingedAddresses );
    request.insertIntegrity( m_stunAuthorization->systemId, m_stunAuthorization->key );

    m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, Message response )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto error = AsyncClient::hasError( code, response ) )
        {
            NX_LOGX( *error, cl_logERROR );
            m_pingedAddresses.clear();
            m_publishedAddresses.clear();
            return setupUpdateTimer();
        }

        m_publishedAddresses = m_pingedAddresses;
        NX_LOGX( lm( "Published addresses: %1" )
                 .container( m_publishedAddresses ), cl_logDEBUG1 );

        setupUpdateTimer();
    } );
}

void MediatorAddressPublisher::terminate()
{
    {
        QnMutexLocker lk( &m_mutex );
        isTerminating = true;
    }

    m_timerSocket->pleaseStopSync();
}

} // namespase cc
} // namespase nx
