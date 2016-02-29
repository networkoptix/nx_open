#include "address_resolver.h"

#include "common/common_globals.h"
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/cc/custom_stun.h>

static const auto DNS_CACHE_TIME = std::chrono::seconds( 10 );
static const auto MEDIATOR_CACHE_TIME = std::chrono::minutes( 1 );

namespace nx {
namespace network {
namespace cloud {

QString toString( const AddressType& type )
{
    switch( type )
    {
        case AddressType::unknown:  return lit( "unknown" );
        case AddressType::regular:  return lit( "regular" );
        case AddressType::cloud:    return lit( "cloud" );
    };

    NX_ASSERT( false, Q_FUNC_INFO, "undefined AddressType" );
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

bool AddressAttribute::operator <( const AddressAttribute& rhs ) const
{
    return type < rhs.type && value < rhs.value;
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

    NX_ASSERT( false, Q_FUNC_INFO, "undefined AddressAttributeType" );
    return lit( "undefined=%1" ).arg( static_cast< int >( type ) );
}

AddressEntry::AddressEntry( AddressType type_, HostAddress host_ )
    : type( type_ )
    , host( std::move( host_ ) )
{
}

AddressEntry::AddressEntry( const SocketAddress& address )
    : type(AddressType::regular)
    , host(address.address)
{
    attributes.push_back(AddressAttribute(
        AddressAttributeType::nxApiPort, address.port));
}

bool AddressEntry::operator ==( const AddressEntry& rhs ) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;
}

bool AddressEntry::operator <( const AddressEntry& rhs ) const
{
    return type < rhs.type && host < rhs.host && attributes < rhs.attributes;
}

QString AddressEntry::toString() const
{
    return lit("%1:%2%3")
            .arg( ::nx::network::cloud::toString( type ) ).arg( host.toString() )
            .arg( containerString( attributes,
                                   lit(","), lit("("), lit(")"), lit("") ) );
}

AddressResolver::AddressResolver(
        std::shared_ptr<hpm::api::MediatorClientTcpConnection> mediatorConnection)
    : m_mediatorConnection(std::move(mediatorConnection))
{
}

void AddressResolver::addFixedAddress( const HostAddress& hostName,
                                       const SocketAddress& hostAddress )
{
    NX_ASSERT(!hostName.isResolved(), Q_FUNC_INFO, "Hostname should be unresolved");

    QnMutexLocker lk(&m_mutex);
    AddressEntry entry(hostAddress);
    auto& entries = m_info[hostName].fixedEntries;
    const auto it = std::find(entries.begin(), entries.end(), entry);
    if (it == entries.end())
    {
        NX_LOGX(lit("New fixed address for %1: %2" )
                .arg(hostName.toString())
                .arg(hostAddress.toString()), cl_logDEBUG1);

        entries.push_back(std::move(entry));

        // we possibly could also grabHandlers to bring the result before DNS is
        // resolved (if in progress), but it does not cause a serious delay so far
    }
}

void AddressResolver::removeFixedAddress( const HostAddress& hostName,
                                          const SocketAddress& hostAddress )
{
    QnMutexLocker lk(&m_mutex);
    AddressEntry entry(hostAddress);
    auto& entries = m_info[hostName].fixedEntries;
    const auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end())
    {
        NX_LOGX(lit("Removed fixed address for %1: %2" )
                .arg(hostName.toString())
                .arg(hostAddress.toString()), cl_logDEBUG1);

        entries.erase(it);
    }
}

void AddressResolver::resolveDomain(
    const HostAddress& domain, std::function<void(std::vector<HostAddress>)> handler )
{
    std::vector<HostAddress> subdomains;
    const auto suffix = lit(".") + domain.toString();
    {
        // TODO: #mux Think about better representation to increase performance
        QnMutexLocker lk( &m_mutex );
        for (const auto& info : m_info)
            if (info.first.toString().endsWith(suffix) &&
                !info.second.getAll().empty())
            {
                subdomains.push_back(info.first);
            }
    }

    // TODO: #mux We also should resolve these in mediator and fire handler later
    handler(std::move(subdomains));
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
    if( hostName.isResolved() )
        return handler( SystemError::noError,
                        std::vector< AddressEntry >( 1, AddressEntry(
                            AddressType::regular, hostName ) ) );

    QnMutexLocker lk( &m_mutex );
    auto info = m_info.emplace( hostName, HostAddressInfo() ).first;
    info->second.checkExpirations();
    if( info->second.isResolved( natTraversal ) )
    {
        auto entries = info->second.getAll();
        lk.unlock();
        NX_LOGX( lit( "Address %1 resolved from cache: %2" )
                 .arg( hostName.toString() )
                 .arg( containerString( entries ) ), cl_logDEBUG2 );

        return handler( SystemError::noError, std::move( entries ) );
    }

    info->second.pendingRequests.insert( requestId );
    m_requests.insert( std::make_pair( requestId,
        RequestInfo( info->first, natTraversal, std::move( handler ) ) ) );

    NX_LOGX( lit( "Address %1 will be resolved later by request %2" )
             .arg( hostName.toString() )
             .arg( reinterpret_cast< size_t >( requestId ) ), cl_logDEBUG1 );

    // Initiate resolution only if it is not initeated yet
    // NOTE: in case of failure or information outdates state will be dropped to init

    if( info->second.dnsState() == HostAddressInfo::State::unresolved )
        dnsResolve( info );

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
            NX_LOGX( lit( "Address %1 could not be resolved: %2" )
                     .arg( hostName.toString() )
                     .arg( SystemError::toString( code ) ), cl_logERROR );

        promise.set_value( std::move( entries ) );
    };

    resolveAsync( hostName, std::move( handler), natTraversal );
    return promise.get_future().get();
}

void AddressResolver::cancel( void* requestId, nx::utils::MoveOnlyFunc< void() > handler )
{
    boost::optional< std::promise< bool > > promise;
    if( !handler )
    {
        // no handler means we have to wait for complete
        promise = std::promise< bool >();
        handler = [ & ](){ promise->set_value( true ); };
    }

    {
        BarrierHandler barrier( std::move( handler ) );
        QnMutexLocker lk( &m_mutex );
        const auto range = m_requests.equal_range( requestId );
        for( auto it = range.first; it != range.second; )
        {
            NX_ASSERT( !it->second.guard, Q_FUNC_INFO,
                        "cancel has already been called for this requestId" );

            if( it->second.inProgress && !it->second.guard )
                ( it++ )->second.guard = barrier.fork();
            else
                it = m_requests.erase( it );
        }
    }

    if( promise )
        promise->get_future().wait();
}

bool AddressResolver::isRequestIdKnown( void* requestId ) const
{
    QnMutexLocker lk( &m_mutex );
    return m_requests.count( requestId );
}

void AddressResolver::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    // TODO: make DnsResolver QnStoppableAsync
    m_dnsResolver.pleaseStop();

    QnMutexLocker lk( &m_mutex );
    if( m_mediatorConnection )
    {
        lk.unlock();
        m_mediatorConnection->pleaseStop( std::move(handler) );
    }
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
        m_dnsResolveTime + DNS_CACHE_TIME < std::chrono::system_clock::now() )
    {
        m_dnsState = State::unresolved;
        m_dnsEntries.clear();
    }

    if( m_mediatorState == State::resolved &&
        m_mediatorResolveTime + MEDIATOR_CACHE_TIME < std::chrono::system_clock::now() )
    {
        m_mediatorState = State::unresolved;
        m_mediatorEntries.clear();
    }
}

bool AddressResolver::HostAddressInfo::isResolved( bool natTraversal ) const
{
    if( !fixedEntries.empty() )
        return true; // fixed entries are in priority

    if( m_dnsState != State::resolved )
        return false; // DNS is required

    if( !m_dnsEntries.empty() )
        return true; // do not wait for mediator if DNS records are avaliable

    return ( !natTraversal || m_mediatorState == State::resolved );
}

std::vector< AddressEntry > AddressResolver::HostAddressInfo::getAll() const
{
    std::vector< AddressEntry > entries( fixedEntries );
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

AddressResolver::RequestInfo::RequestInfo(RequestInfo&& right)
:
    address(std::move(right.address)),
    inProgress(right.inProgress),
    natTraversal(right.natTraversal),
    handler(std::move(right.handler)),
    guard(std::move(right.guard))
{
}

void AddressResolver::dnsResolve( HaInfoIterator info )
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
            NX_LOGX( lit( "Address %1 is resolved to %2" )
                     .arg( info->first.toString() )
                     .arg( hostAddress.toString() ), cl_logDEBUG1 );
        }
        else
        {
            // count failure as unresolvable, better luck next time
            NX_LOGX( lit( "Address %1 could not be resolved: %2" )
                     .arg( info->first.toString() )
                     .arg( SystemError::toString( code ) ), cl_logDEBUG1 );
        }

        info->second.setDnsEntries( entries );
        guards = grabHandlers( code, info, &lk );
    };

    info->second.dnsProgress();
    m_dnsResolver.resolveAddressAsync( info->first, std::move( functor ), this );
}

void AddressResolver::mediatorResolve( HaInfoIterator info, QnMutexLockerBase* lk )
{
    info->second.mediatorProgress();
    lk->unlock();
    m_mediatorConnection->resolve(
        nx::hpm::api::ResolveRequest(info->first.toString().toUtf8()),
        [this, info](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolveResponse response)
        {
            std::vector< Guard > guards;

            QnMutexLocker lk( &m_mutex );
            std::vector< AddressEntry > entries;
            if (resultCode == nx::hpm::api::ResultCode::ok)
            {
                for (const auto& it : response.endpoints)
                {
                    AddressEntry entry( AddressType::regular, it.address );
                    entry.attributes.push_back( AddressAttribute(
                        AddressAttributeType::nxApiPort, it.port ) );
                    entries.push_back( std::move( entry ) );
                }

                //if target host supports cloud connect, adding corresponding address entry
                if (response.connectionMethods != 0)
                    entries.emplace_back(
                        AddressType::cloud,
                        HostAddress(info->first.toString()));
            }

            info->second.setMediatorEntries(std::move(entries));
            guards = grabHandlers( SystemError::noError, info, &lk );
        });
}

std::vector< Guard > AddressResolver::grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info,
        QnMutexLockerBase* /*lk*/ )
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
            if( it->second.address != info->first ||
                it->second.inProgress ||
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
                NX_LOGX( lit( "Address %1 is resolved by request %2 to %3" )
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

    NX_LOGX( lit( "There are %1 about to be notified: %2 is resolved to %3" )
             .arg( guards.size() ).arg( info->first.toString() )
             .arg( containerString( entries ) ), cl_logDEBUG1 );
    return guards;
}

} // namespace cloud
} // namespace network
} // namespace nx
