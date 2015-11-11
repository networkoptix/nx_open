#include "mediator_address_publisher.h"

#include "common/common_globals.h"
#include "utils/common/log.h"
#include "utils/network/socket_global.h"
#include "utils/network/stun/cc/custom_stun.h"

namespace nx {
namespace cc {

const TimerDuration MediatorAddressPublisher::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 10 );

MediatorAddressPublisher::MediatorAddressPublisher( String serverId,
                                                    TimerDuration updateInterval )
    : m_serverId( std::move( serverId ) )
    , m_updateInterval( std::move( updateInterval ) )
    , isTerminating( false )
{
    SocketGlobals::mediatorConnector().enable();
}

MediatorAddressPublisher::~MediatorAddressPublisher()
{
    QnMutexLocker lk( &m_mutex );
    isTerminating = true;

    const auto client = std::move( m_stunClient );
    const auto guard = std::move( m_timerGuard );

    lk.unlock();
}

bool MediatorAddressPublisher::Authorization::operator == ( const Authorization& rhs ) const
{
    return systemId == rhs.systemId && key == rhs.key;
}

void MediatorAddressPublisher::authorizationChanged(
        boost::optional< Authorization > authorization )
{
    QnMutexLocker lk( &m_mutex );
    if( m_stunAuthorization == authorization )
        return;

    NX_LOGX( lm( "New credentials: %1" ).arg(
                 authorization.is_initialized() ? authorization->systemId : String("none") ),
             cl_logDEBUG1 );

    m_stunAuthorization = std::move( authorization );
    if( m_stunClient )
    {
        const auto stunClient = std::move( m_stunClient );
        lk.unlock();
    }
    else
    {
        pingReportedAddresses();
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

    m_timerGuard = TimerManager::instance()->addTimer(
        [ this ]( quint64 )
        {
            QnMutexLocker lk( &m_mutex );
            pingReportedAddresses();
        },
        m_updateInterval );
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

        m_stunClient->openConnection(
            [ this ]( SystemError::ErrorCode code )
            {
                if( code == SystemError::noError )
                    NX_LOGX( lit( "Mediator connection is estabilished" ),
                             cl_logDEBUG1 );
            },
            [ this ]( stun::Message indication )
            {
                static_cast< void >( indication ); // TODO
            },
            [ this ]( SystemError::ErrorCode code )
            {
                NX_LOGX( lit( "Mediator connection has been lost: %1" )
                         .arg( SystemError::toString( code ) ),
                         cl_logDEBUG1 );

                QnMutexLocker lk( &m_mutex );
                m_publishedAddresses.clear();
            });
    }

    return m_stunClient && m_stunAuthorization;
}

void MediatorAddressPublisher::pingReportedAddresses()
{
    if( !isCloudReady() )
        return setupUpdateTimer();

    if( m_reportedAddresses == m_pingedAddresses )
        return publishPingedAddresses();

    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::ping ) );
    request.newAttribute< stun::cc::attrs::SystemId >( m_stunAuthorization->systemId );
    request.newAttribute< stun::cc::attrs::ServerId >( m_serverId );
    request.newAttribute< stun::cc::attrs::PublicEndpointList >( m_reportedAddresses );
    request.insertIntegrity( m_stunAuthorization->systemId, m_stunAuthorization->key );

    m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, stun::Message response )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto error = stun::AsyncClient::hasError( code, response ) )
        {
            NX_LOGX( *error, cl_logERROR );
            m_pingedAddresses.clear();
            return setupUpdateTimer();
        }

        const auto eps = response.getAttribute< stun::cc::attrs::PublicEndpointList >();
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

    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::bind ) );
    request.newAttribute< stun::cc::attrs::SystemId >( m_stunAuthorization->systemId );
    request.newAttribute< stun::cc::attrs::ServerId >( m_serverId );
    request.newAttribute< stun::cc::attrs::PublicEndpointList >( m_pingedAddresses );
    request.insertIntegrity( m_stunAuthorization->systemId, m_stunAuthorization->key );

    m_stunClient->sendRequest(
        std::move( request ),
        [ this ]( SystemError::ErrorCode code, stun::Message response )
    {
        QnMutexLocker lk( &m_mutex );
        if( const auto error = stun::AsyncClient::hasError( code, response ) )
        {
            NX_LOGX( *error, cl_logERROR );
            return setupUpdateTimer();
        }

        m_publishedAddresses = m_pingedAddresses;
        NX_LOGX( lm( "Published addresses: %1" )
                 .container( m_publishedAddresses ), cl_logDEBUG1 );

        setupUpdateTimer();
    } );
}

} // namespase cc
} // namespase nx
