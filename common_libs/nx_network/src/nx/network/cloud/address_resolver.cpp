
#include "address_resolver.h"

#include "common/common_globals.h"

#include <nx/utils/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <utils/serialization/lexical.h>


static const auto DNS_CACHE_TIME = std::chrono::seconds(10);
static const auto MEDIATOR_CACHE_TIME = std::chrono::seconds(10);

namespace nx {
namespace network {
namespace cloud {

QString toString(const AddressType& type)
{
    switch(type)
    {
        case AddressType::unknown: return lit("unknown");
        case AddressType::direct: return lit("direct");
        case AddressType::cloud: return lit("cloud");
    };

    NX_ASSERT( false, Q_FUNC_INFO, "undefined AddressType" );
    return lit( "undefined=%1" ).arg( static_cast< int >( type ) );
}

TypedAddress::TypedAddress(HostAddress address_, AddressType type_)
:
    address(std::move(address_)),
    type(std::move(type_))
{
}

QString TypedAddress::toString() const
{
    return lm("%1(%2)").str(address).str(type);
}

AddressAttribute::AddressAttribute(AddressAttributeType type_, quint64 value_)
    : type( type_ )
    , value( value_ )
{
}

bool AddressAttribute::operator ==(const AddressAttribute& rhs) const
{
    return type == rhs.type && value == rhs.value;
}

bool AddressAttribute::operator <(const AddressAttribute& rhs) const
{
    return type < rhs.type && value < rhs.value;
}

QString AddressAttribute::toString() const
{
    switch(type)
    {
        case AddressAttributeType::unknown:
            return lit("unknown");
        case AddressAttributeType::port:
            return lit("port=%1").arg(value);
    };

    NX_ASSERT( false, Q_FUNC_INFO, "undefined AddressAttributeType" );
    return lit( "undefined=%1" ).arg( static_cast< int >( type ) );
}

AddressEntry::AddressEntry(AddressType type_, HostAddress host_)
    : type( type_ )
    , host( std::move( host_ ) )
{
}

AddressEntry::AddressEntry(const SocketAddress& address)
    : type(AddressType::direct)
    , host(address.address)
{
    attributes.push_back(AddressAttribute(
        AddressAttributeType::port, address.port));
}

bool AddressEntry::operator ==(const AddressEntry& rhs) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;
}

bool AddressEntry::operator <(const AddressEntry& rhs) const
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
:
    m_mediatorConnection(std::move(mediatorConnection))
{
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const SocketAddress& hostAddress )
{
    NX_ASSERT(!hostName.isResolved(), Q_FUNC_INFO, "Hostname should be unresolved");
    NX_ASSERT(hostAddress.address.isResolved());

    NX_LOGX(lit("Added fixed address for %1: %2")
        .arg(hostName.toString())
        .arg(hostAddress.toString()), cl_logDEBUG2);

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

        // we possibly could also grabHandlers() to bring the result before DNS
        // is resolved (if in progress), but it does not cause a serious delay
        // so far
    }
}

void AddressResolver::removeFixedAddress(
    const HostAddress& hostName, const SocketAddress& hostAddress)
{

    NX_LOGX(lit("Removed fixed address for %1: %2")
        .arg(hostName.toString())
        .arg(hostAddress.toString()), cl_logDEBUG2);

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
    const HostAddress& domain,
    utils::MoveOnlyFunc<void(std::vector<TypedAddress>)> handler)
{
    m_mediatorConnection->resolveDomain(
        nx::hpm::api::ResolveDomainRequest(domain.toString().toUtf8()),
        [this, domain, handler = std::move(handler)](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolveDomainResponse response)
        {
            NX_LOGX(lm("Domain %1 resolution on mediator result: %2")
                .arg(domain.toString()).arg(QnLexical::serialized(resultCode)),
                cl_logDEBUG2);

            std::vector<TypedAddress> result;
            const auto suffix = lit(".") + domain.toString();
            {
                // TODO: #mux Think about better representation to increase performance
                QnMutexLocker lk( &m_mutex );
                for (const auto& info : m_info)
                    if (info.first.toString().endsWith(suffix) &&
                        !info.second.fixedEntries.empty())
                    {
                        result.emplace_back(info.first, AddressType::direct);
                    }
            }

            for (const auto& host : response.hostNames)
            {
                HostAddress address(QString::fromUtf8(host));
                result.emplace_back(std::move(address), AddressType::cloud);
            }

            NX_LOGX(lm("Domain %1 is resolved to: %2")
                .arg(domain.toString()).container(result), cl_logDEBUG1);

            handler(std::move(result));
        });
}

void AddressResolver::resolveAsync(
    const HostAddress& hostName, ResolveHandler handler,
    bool natTraversal, void* requestId)
{
    if( hostName.isResolved() )
        return handler( SystemError::noError,
                        std::vector< AddressEntry >( 1, AddressEntry(
                            AddressType::direct, hostName ) ) );

    QnMutexLocker lk( &m_mutex );
    auto info = m_info.emplace( hostName, HostAddressInfo() ).first;
    info->second.checkExpirations();
    tryFastDomainResolve(info);
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

std::vector<AddressEntry> AddressResolver::resolveSync(
     const HostAddress& hostName, bool natTraversal = false)
{
    nx::utils::promise< std::vector< AddressEntry > > promise;
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

void AddressResolver::cancel(
    void* requestId, nx::utils::MoveOnlyFunc<void()> handler)
{
    boost::optional< nx::utils::promise< bool > > promise;
    if( !handler )
    {
        // no handler means we have to wait for complete
        promise = nx::utils::promise< bool >();
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

bool AddressResolver::isRequestIdKnown(void* requestId) const
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
    std::vector<AddressEntry> entries )
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

bool AddressResolver::HostAddressInfo::isResolved(bool natTraversal) const
{
    if(!fixedEntries.empty() || !m_dnsEntries.empty() || !m_mediatorEntries.empty())
        return true; // any address is better than nothing

    return (m_dnsState == State::resolved) &&
        (!natTraversal || m_mediatorState == State::resolved);
}

std::vector<AddressEntry> AddressResolver::HostAddressInfo::getAll() const
{
    std::vector< AddressEntry > entries( fixedEntries );
    entries.insert( entries.end(), m_dnsEntries.begin(), m_dnsEntries.end() );
    entries.insert( entries.end(), m_mediatorEntries.begin(), m_mediatorEntries.end() );
    return entries;
}

AddressResolver::RequestInfo::RequestInfo(
    HostAddress _address, bool _natTraversal, ResolveHandler _handler)
:
    address( std::move( _address ) ),
    inProgress( false ),
    natTraversal( _natTraversal ),
    handler( std::move( _handler ) )
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

void AddressResolver::tryFastDomainResolve(HaInfoIterator info)
{
    const auto domain = info->first.toString();
    if (domain.indexOf(lit(".")) != -1)
        return; // only top level domains might be fast resolved

    // TODO: #mux Think about better representation to increase performance
    const auto suffix = lit(".") + domain;
    for (const auto& other : m_info)
    {
        if (other.first.toString().endsWith(suffix) &&
            !other.second.fixedEntries.empty())
        {
            info->second.fixedEntries = other.second.fixedEntries;
            return; // just resolve to first avaliable
        }
    }

    info->second.fixedEntries.clear();
}

void AddressResolver::dnsResolve(HaInfoIterator info)
{
    auto functor = [ = ]( SystemError::ErrorCode code, const HostAddress& host )
    {
        const HostAddress hostAddress( host.inAddr() );
        std::vector< Guard > guards;

        QnMutexLocker lk( &m_mutex );
        std::vector< AddressEntry > entries;
        if( code == SystemError::noError )
        {
            entries.push_back( AddressEntry( AddressType::direct, hostAddress ) );
            NX_LOGX( lit( "Address %1 is resolved to %2" )
                     .arg( info->first.toString() )
                     .arg( hostAddress.toString() ), cl_logDEBUG1 );
        }

        info->second.setDnsEntries( entries );
        guards = grabHandlers( code, info, &lk );
    };

    info->second.dnsProgress();
    m_dnsResolver.resolveAddressAsync( info->first, std::move( functor ), this );
}

void AddressResolver::mediatorResolve(HaInfoIterator info, QnMutexLockerBase* lk)
{
    info->second.mediatorProgress();
    lk->unlock();
    m_mediatorConnection->resolvePeer(
        nx::hpm::api::ResolvePeerRequest(info->first.toString().toUtf8()),
        [this, info](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolvePeerResponse response)
        {
            std::vector< Guard > guards;

            QnMutexLocker lk( &m_mutex );
            std::vector< AddressEntry > entries;
            if (resultCode == nx::hpm::api::ResultCode::ok)
            {
                for (const auto& it : response.endpoints)
                {
                    AddressEntry entry( AddressType::direct, it.address );
                    entry.attributes.push_back( AddressAttribute(
                        AddressAttributeType::port, it.port ) );
                    entries.push_back( std::move( entry ) );
                }

                // if target host supports cloud connect, adding corresponding
                // address entry
                if (response.connectionMethods != 0)
                    entries.emplace_back(
                        AddressType::cloud,
                        HostAddress(info->first.toString()));
            }

            info->second.setMediatorEntries(std::move(entries));
            guards = grabHandlers(
                resultCode == nx::hpm::api::ResultCode::ok
                    ? SystemError::noError
                    : SystemError::hostUnreach, //TODO #ak correct error translation
                info,
                &lk);
        });
}

std::vector<Guard> AddressResolver::grabHandlers(
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
                auto resolveDataPair = std::move(*it);
                {
                    QnMutexLocker lk(&m_mutex);
                    m_requests.erase(it);
                }

                auto code = entries.empty() ? lastErrorCode : SystemError::noError;
                resolveDataPair.second.handler( code, std::move( entries ) );

                QnMutexLocker lk( &m_mutex );
                NX_LOGX( lit( "Address %1 is resolved by request %2 to %3" )
                         .arg( info->first.toString() )
                         .arg( reinterpret_cast< size_t >(resolveDataPair.first ) )
                         .arg( containerString( entries ) ), cl_logDEBUG2 );
                m_condition.wakeAll();
            } ) );
        }

        if( noPending )
            req = info->second.pendingRequests.erase( req );
        else
            ++req;
    }

    if (guards.size())
    {
        NX_LOGX( lit( "There is(are) %1 about to be notified: %2 is resolved to %3" )
                 .arg( guards.size() ).arg( info->first.toString() )
                 .arg( containerString( entries ) ), cl_logDEBUG1 );
    }

    return guards;
}

} // namespace cloud
} // namespace network
} // namespace nx
