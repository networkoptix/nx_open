#include "address_resolver.h"

#include "common/common_globals.h"
#include "utils/common/log.h"
#include "utils/network/stun/cc/custom_stun.h"

static const SocketAddress MEDIATOR_ADDRESS( lit( "localhost" ) );

namespace nx {
namespace cc {

AddressAttribute::AddressAttribute( AddressAttributeType type_, quint64 value_ )
    : type( type_ )
    , value( value_ )
{
}

bool AddressAttribute::operator ==( const AddressAttribute& rhs ) const
{
    return type == rhs.type && value == rhs.value;
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

AddressResolver::AddressResolver()
    : m_stunClient( MEDIATOR_ADDRESS )
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
static QString containerToString( const T& container )
{
    QString str;
    for( const auto& each : container )
        str += lit(" ") + each.host.toString();
    return str;
}

bool AddressResolver::resolveAsync(
        const HostAddress& hostName,
        std::function< void( std::vector< AddressEntry > ) > handler,
        bool natTraversal, void* requestId )
{
    if( hostName.isResolved() )
    {
        NX_LOG( lit( "%1 address %2 is already resolved" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( hostName.toString() ),
                cl_logDEBUG2 );
        handler( std::vector< AddressEntry >( 1, AddressEntry(
            AddressType::regular, hostName ) ) );
        return true;
    }

    QnMutexLocker lk( &m_mutex );
    auto info = m_info.emplace( hostName, HostAddressInfo() ).first;
    if( info->second.isResolved( natTraversal ) )
    {
        auto entries = info->second.getAll();
        lk.unlock();
        NX_LOG( lit( "%1 address %2 resolved from cache: %3" )
                .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( hostName.toString() )
                .arg( containerToString( entries ) ), cl_logDEBUG2 );

        handler( std::move( entries ) );
        return true;
    }

    info->second.pendingRequests.insert( requestId );
    m_requests.insert( std::make_pair( requestId,
        RequestInfo( info->first, natTraversal, std::move( handler ) ) ) );

    NX_LOG( lit( "%1 address %2 will be resolved later by request %3" )
            .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( hostName.toString() )
            .arg( reinterpret_cast< size_t >( requestId ) ), cl_logDEBUG1 );

    // Initiate resolution only if it is not initeated yet
    // NOTE: in case of failure or information outdates state will be dropped to init

    if( info->second.dnsState == HostAddressInfo::State::unresolved )
        if( !dnsResolve( info, &lk ) )
            return false;

    if( natTraversal && info->second.mediatorState == HostAddressInfo::State::unresolved )
        if( !mediatorResolve( info, &lk ) )
            return false;

    return true;
}

std::vector< AddressEntry > AddressResolver::resolveSync( const HostAddress& hostName,
                                                          bool natTraversal = false )
{
    std::promise< std::vector< AddressEntry > > promise;
    if( !resolveAsync( hostName, [ & ]( std::vector< AddressEntry > entry )
                       {
                        promise.set_value( std::move( entry ) );
                       },
                       natTraversal ) )
        return std::vector< AddressEntry >();

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

AddressResolver::HostAddressInfo::HostAddressInfo()
    : dnsState( State::unresolved )
    , mediatorState( State::unresolved )
{
}

bool AddressResolver::HostAddressInfo::isResolved( bool natTraversal ) const
{
    return dnsState == State::resolved &&
           (!natTraversal || mediatorState == State::resolved);
}

std::vector< AddressEntry > AddressResolver::HostAddressInfo::getAll() const
{
    std::vector< AddressEntry > entries( peers );
    if( dnsResult )
        entries.push_back( *dnsResult );

    entries.insert( entries.end(), mediatorResult.begin(), mediatorResult.end() );
    return std::move( entries );
}

AddressResolver::RequestInfo::RequestInfo(
        const HostAddress& _address, bool _natTraversal,
        std::function< void( std::vector< AddressEntry > ) > _handler )
    : address( _address )
    , inProgress( false )
    , natTraversal( _natTraversal )
    , handler( std::move( _handler ) )
{
}

bool AddressResolver::dnsResolve( HaInfoIterator info, QnMutexLockerBase* lk )
{
    auto functor = [ = ]( SystemError::ErrorCode code, const HostAddress& host )
    {
        const HostAddress hostAddress( host.inAddr() );
        std::vector< Guard > guards;

        QnMutexLocker lk( &m_mutex );
        info->second.dnsState = HostAddressInfo::State::resolved;
        if( code == SystemError::noError )
        {
            info->second.dnsResult = AddressEntry( AddressType::regular, hostAddress );
            NX_LOG( lit( "%1 address %2 is resolved to %3" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( info->first.toString() )
                    .arg( hostAddress.toString() ), cl_logDEBUG1 );
        }
        else
        {
            // count failure as unresolvable, better luck next time
            info->second.dnsResult = boost::none;
            NX_LOG( lit( "%1 address %2 could not be resolved: %3" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( info->first.toString() )
                    .arg( SystemError::toString( code ) ), cl_logERROR );
        }

        guards = grabHandlers( info );
    };

    info->second.dnsState = HostAddressInfo::State::inProgress;
    lk->unlock();
    if( !m_dnsResolver.resolveAddressAsync( info->first, std::move( functor ), this ) )
    {
        lk->relock();
        info->second.dnsState = HostAddressInfo::State::unresolved;
        return false;
    }

    lk->relock();
    return true;
}

bool AddressResolver::mediatorResolve( HaInfoIterator info, QnMutexLockerBase* lk )
{
    stun::Message request( stun::Header( stun::MessageClass::request,
                                         stun::cc::methods::connect ) );
    request.newAttribute< stun::cc::attrs::ClientId >("SomeClientId" ); // TODO
    request.newAttribute< stun::cc::attrs::HostName >(info->first.toString().toUtf8() );

    auto functor = [ = ]( SystemError::ErrorCode code, stun::Message message )
    {
        std::vector< Guard > guards;

        QnMutexLocker lk( &m_mutex );
        info->second.mediatorState = HostAddressInfo::State::resolved;
        info->second.mediatorResult.clear();
        if( const auto error = stun::AsyncClient::hasError( code, message ) )
        {
            // count failure as unresolvable, better luck next time
            NX_LOG( lit( "%1 %2" ).arg( QString::fromUtf8( Q_FUNC_INFO ) )
                                  .arg( *error ), cl_logERROR );
        }
        else
        if( auto eps = message.getAttribute< stun::cc::attrs::PublicEndpointList >() )
        {
            for( const auto& it : eps->get() )
            {
                AddressEntry entry( AddressType::regular, it.address );
                entry.attributes.push_back( AddressAttribute(
                    AddressAttributeType::nxApiPort, it.port ) );
                info->second.mediatorResult.push_back( std::move( entry ) );
            }
        }

        guards = grabHandlers( info );
    };

    info->second.mediatorState = HostAddressInfo::State::inProgress;
    lk->unlock();
    if( !m_stunClient.sendRequest( std::move( request ), std::move( functor ) ) )
    {
        lk->relock();
        info->second.mediatorState = HostAddressInfo::State::unresolved;
        return false;
    }

    lk->relock();
    return true;
}

std::vector< Guard > AddressResolver::grabHandlers( HaInfoIterator info )
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
                it->second.handler( std::move( entries ) );

                QnMutexLocker lk( &m_mutex );
                NX_LOG( lit( "%1 address %2 is resolved to request %3 to:%4" )
                        .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( info->first.toString() )
                        .arg( reinterpret_cast< size_t >( it->first ) )
                        .arg( containerToString( entries ) ), cl_logDEBUG2 );

                m_requests.erase( it );
                m_condition.wakeAll();
            } ) );
        }

        if( noPending )
            req = info->second.pendingRequests.erase( req );
        else
            ++req;
    }

    NX_LOG( lit( "%1 there are %2 about to be notified:%3" )
            .arg( QString::fromUtf8( Q_FUNC_INFO ) ).arg( guards.size() )
            .arg( containerToString( entries ) ), cl_logDEBUG1 );
    return guards;
}

} // namespace cc
} // namespace nx
