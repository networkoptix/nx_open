#include "address_resolver.h"

#include "common/common_globals.h"
#include "utils/common/log.h"
#include "utils/network/stun/cc/custom_stun.h"

static const SocketAddress MEDIATOR_ADDRESS( lit( "localhost" ) );

static const auto DNS_CACHE_TIME = std::chrono::seconds( 10 );
static const auto MEDIATOR_CACHE_TIME = std::chrono::minutes( 1 );

namespace nx {
namespace cc {

QString toString( const AddressType& type )
{
    switch( type )
    {
        case AddressType::unknown:  return lit( "unknown" );
        case AddressType::regular:  return lit( "unknown" );
        case AddressType::cloud:    return lit( "unknown" );
    };

    Q_ASSERT_X( false, Q_FUNC_INFO, "undefined AddressType" );
    return lit( "undefined=%1" ).arg( static_cast< int >( type ) );
}

AddressAttribute::AddressAttribute( AddressAttributeType type_, quint64 value_ )
    : type( type_ )
    , value( value_ )
{
}

bool AddressAttribute::operator ==( const AddressAttribute& rhs ) const
{
    return type == rhs.type && value == rhs.value;
}

QString AddressAttribute::toString() const
{
    switch( type )
    {
        case AddressAttributeType::peerFoundPassively:
            return lit( "peerFoundPassively" );

        case AddressAttributeType::nxApiPort:
            return lit( "nxApiPort=%1" ).arg( value );
    };

    Q_ASSERT_X( false, Q_FUNC_INFO, "undefined AddressAttributeType" );
    return lit( "undefined=%1" ).arg( static_cast< int >( type ) );
}

AddressEntry::AddressEntry( AddressType type_, HostAddress host_ )
    : type( type_ )
    , host( std::move( host_ ) )
{
}

bool AddressEntry::operator ==( const AddressEntry& rhs ) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;

}

QString AddressEntry::toString() const
{
    return lit("%1:%2%3")
            .arg( ::nx::cc::toString( type ) ).arg( host.toString() )
            .arg( containerString( attributes,
                                   lit(","), lit("("), lit(")"), lit("") ) );
}

AddressResolver::AddressResolver()
    : m_stunClient( new stun::AsyncClient( MEDIATOR_ADDRESS ) )
{
}

void AddressResolver::addPeerAddress( const HostAddress& hostName,
                                      HostAddress hostAddress,
                                      std::vector< AddressAttribute > attributes )
{
    Q_ASSERT_X( !hostName.isResolved(), Q_FUNC_INFO, "Hostname should be unresolved" );
    Q_ASSERT_X( hostAddress.isResolved(), Q_FUNC_INFO, "Address should be resolved" );

    AddressEntry address( AddressType::regular, std::move( hostAddress ) );
    address.attributes = std::move( attributes );

    QnMutexLocker lk( &m_mutex );
    auto& hostAdresses = m_info[ hostName ].peers;
    const auto it = std::find( hostAdresses.begin(), hostAdresses.end(), address );
    if( it == hostAdresses.end() )
        hostAdresses.push_back( std::move( address ) );
}

template< typename T >
static QString containerToQString( const T& container )
{
    QString str;
    for( const auto& each : container )
        str += lit(" ") + each.toString();
    return str;
}

void AddressResolver::resolveAsync(
        const HostAddress& hostName, ResolveHandler handler,
        bool natTraversal, void* requestId )
{
    if( hostName.isResolved() ) return handler(
        SystemError::noError,
        std::vector< AddressEntry >( 1, AddressEntry( AddressType::regular, hostName ) ) );

    QnMutexLocker lk( &m_mutex );
    auto info = m_info.emplace( hostName, HostAddressInfo() ).first;
    info->second.checkExpirations();
    if( info->second.isResolved( natTraversal ) )
    {
        auto entries = info->second.getAll();
        lk.unlock();
        NX_LOG( lit( "%1 address %2 resolved from cache: %3" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( hostName.toString() )
                .arg( containerString( entries ) ), cl_logDEBUG2 );

        return handler( SystemError::noError, std::move( entries ) );
    }

    info->second.pendingRequests.insert( requestId );
    m_requests.insert( std::make_pair( requestId,
        RequestInfo( info->first, natTraversal, std::move( handler ) ) ) );

    NX_LOG( lit( "%1 address %2 will be resolved later by request %3" )
            .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( hostName.toString() )
            .arg( reinterpret_cast< size_t >( requestId ) ), cl_logDEBUG1 );

    // Initiate resolution only if it is not initeated yet
    // NOTE: in case of failure or information outdates state will be dropped to init

    if( info->second.dnsState() == HostAddressInfo::State::unresolved )
        dnsResolve( info, &lk );

    if( natTraversal &&
        info->second.mediatorState() == HostAddressInfo::State::unresolved )
            mediatorResolve( info, &lk );
}

std::vector< AddressEntry > AddressResolver::resolveSync(
        const HostAddress& hostName, bool natTraversal = false )
{
    std::promise< std::vector< AddressEntry > > promise;
    auto handler = [ & ]( SystemError::ErrorCode code,
                          std::vector< AddressEntry > entries )
    {
        if( code != SystemError::noError )
            NX_LOG( lit( "%1 address %2 could not be resolved: %3" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( hostName.toString() )
                    .arg( SystemError::toString( code ) ), cl_logERROR );

        promise.set_value( std::move( entries ) );
    };

    resolveAsync( hostName, std::move( handler), natTraversal );
    return promise.get_future().get();
}

void AddressResolver::cancel( void* requestId, bool waitForRunningHandlerCompletion )
{
    QnMutexLocker lk( &m_mutex );
    bool needToWait = false;
    for( ;; )
    {
        const auto range = m_requests.equal_range( requestId );
        for( auto it = range.first; it != range.second; )
        {
            if( waitForRunningHandlerCompletion && it->second.inProgress )
            {
                needToWait = true;
                ++it;
            }
            else
            {
                // remove all we dont have to wait
                it = m_requests.erase( it );
            }
        }

        if( !needToWait )
            return;

        // wait for some tasks to complete and start all over again
        m_condition.wait( &m_mutex );
    }
}

bool AddressResolver::isRequestIdKnown( void* requestId ) const
{
    QnMutexLocker lk( &m_mutex );
    return m_requests.count( requestId );
}

void AddressResolver::resetMediatorAddress( const SocketAddress& newAddress )
{
    m_stunClient.reset( new stun::AsyncClient( newAddress ) );
}

AddressResolver::HostAddressInfo::HostAddressInfo()
    : m_dnsState( State::unresolved )
    , m_mediatorState( State::unresolved )
{
}

void AddressResolver::HostAddressInfo::setDnsEntries(
        std::vector< AddressEntry > entries )
{
    m_dnsState = State::resolved;
    m_dnsResolveTime = std::chrono::system_clock::now();
    m_dnsEntries = std::move( entries );
}

void AddressResolver::HostAddressInfo::setMediatorEntries(
        std::vector< AddressEntry > entries )
{
    m_mediatorState = State::resolved;
    m_mediatorResolveTime = std::chrono::system_clock::now();
    m_mediatorEntries = std::move( entries );
}

void AddressResolver::HostAddressInfo::checkExpirations()
{
    if( m_dnsState == State::resolved &&
        m_dnsResolveTime + DNS_CACHE_TIME > std::chrono::system_clock::now() )
    {
        m_dnsState = State::unresolved;
        m_dnsEntries.clear();
    }

    if( m_mediatorState == State::resolved &&
        m_mediatorResolveTime + MEDIATOR_CACHE_TIME > std::chrono::system_clock::now() )
    {
        m_mediatorState = State::unresolved;
        m_mediatorEntries.clear();
    }
}

bool AddressResolver::HostAddressInfo::isResolved( bool natTraversal ) const
{
    return m_dnsState == State::resolved &&
           (!natTraversal || m_mediatorState == State::resolved);
}

std::vector< AddressEntry > AddressResolver::HostAddressInfo::getAll() const
{
    std::vector< AddressEntry > entries( peers );
    entries.insert( entries.end(), m_dnsEntries.begin(), m_dnsEntries.end() );
    entries.insert( entries.end(), m_mediatorEntries.begin(), m_mediatorEntries.end() );
    return entries;
}

AddressResolver::RequestInfo::RequestInfo(
        HostAddress _address, bool _natTraversal, ResolveHandler _handler )
    : address( std::move( _address ) )
    , inProgress( false )
    , natTraversal( _natTraversal )
    , handler( std::move( _handler ) )
{
}

void AddressResolver::dnsResolve( HaInfoIterator info, QnMutexLockerBase* lk )
{
    auto functor = [ = ]( SystemError::ErrorCode code, const HostAddress& host )
    {
        const HostAddress hostAddress( host.inAddr() );
        std::vector< Guard > guards;

        QnMutexLocker lk( &m_mutex );
        std::vector< AddressEntry > entries;
        if( code == SystemError::noError )
        {
            entries.push_back( AddressEntry( AddressType::regular, hostAddress ) );
            NX_LOG( lit( "%1 address %2 is resolved to %3" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( info->first.toString() )
                    .arg( hostAddress.toString() ), cl_logDEBUG1 );
        }
        else
        {
            // count failure as unresolvable, better luck next time
            NX_LOG( lit( "%1 address %2 could not be resolved: %3" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( info->first.toString() )
                    .arg( SystemError::toString( code ) ), cl_logDEBUG1 );
        }

        info->second.setDnsEntries( entries );
        guards = grabHandlers( code, info );
    };

    info->second.dnsProgress();
}

void AddressResolver::mediatorResolve( HaInfoIterator info, QnMutexLockerBase* lk )
{
    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::connect ) );
    request.newAttribute< stun::cc::attrs::ClientId >("SomeClientId" ); // TODO
    request.newAttribute< stun::cc::attrs::HostName >(info->first.toString().toUtf8() );

    auto functor = [ = ]( SystemError::ErrorCode code, stun::Message message )
    {
        std::vector< Guard > guards;

        QnMutexLocker lk( &m_mutex );
        std::vector< AddressEntry > entries;
        if( const auto error = stun::AsyncClient::hasError( code, message ) )
        {
            // count failure as unresolvable, better luck next time
            NX_LOG( lit( "%1 %2" ).arg( QString::fromUtf8( Q_FUNC_INFO ) )
                                  .arg( *error ), cl_logDEBUG1 );

            code = SystemError::dnsServerFailure;
        }
        else
        if( auto eps = message.getAttribute< stun::cc::attrs::PublicEndpointList >() )
        {
            for( const auto& it : eps->get() )
            {
                AddressEntry entry( AddressType::regular, it.address );
                entry.attributes.push_back( AddressAttribute(
                    AddressAttributeType::nxApiPort, it.port ) );
                entries.push_back( std::move( entry ) );
            }
        }

        info->second.setMediatorEntries( entries );
        guards = grabHandlers( code, info );
    };

    info->second.mediatorProgress();
    lk->unlock();
    if( !m_stunClient->sendRequest( std::move( request ), std::move( functor ) ) )
    {
        lk->relock();
        info->second.setMediatorEntries();
        const auto guards = grabHandlers( SystemError::dnsServerFailure, info );
        lk->unlock();
    }
}

std::vector< Guard > AddressResolver::grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info )
{
    std::vector< Guard > guards;

    auto entries = info->second.getAll();
    for( auto req = info->second.pendingRequests.begin();
         req != info->second.pendingRequests.end(); )
    {
        const auto range = m_requests.equal_range( *req );
        bool noPending = ( range.first == range.second );
        for( auto it = range.first; it != range.second; ++it )
        {
            if( it->second.address != info->first &&
                !info->second.isResolved( it->second.natTraversal ) )
            {
                noPending = false;
                continue;
            }

            it->second.inProgress = true;
            guards.push_back( Guard( [ = ]() {
                auto code = entries.empty() ? lastErrorCode : SystemError::noError;
                it->second.handler( code, std::move( entries ) );

                QnMutexLocker lk( &m_mutex );
                NX_LOG( lit( "%1 address %2 is resolved by request %3 to %4" )
                        .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                        .arg( info->first.toString() )
                        .arg( reinterpret_cast< size_t >( it->first ) )
                        .arg( containerString( entries ) ), cl_logDEBUG2 );

                m_requests.erase( it );
                m_condition.wakeAll();
            } ) );
        }

        if( noPending )
            req = info->second.pendingRequests.erase( req );
        else
            ++req;
    }

    NX_LOG( lit( "%1 there are %2 about to be notified: %3 resolved to %4" )
            .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( guards.size() )
            .arg( info->first.toString() ).arg( containerString( entries ) ),
            cl_logDEBUG1 );
    return guards;
}

} // namespace cc
} // namespace nx
