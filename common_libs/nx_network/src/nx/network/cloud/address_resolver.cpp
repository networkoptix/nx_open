#include "address_resolver.h"

#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/fusion/serialization/lexical.h>

#include "mediator_connector.h"

static const auto kDnsCacheTimeout = std::chrono::seconds(10);
static const auto kMediatorCacheTimeout = std::chrono::seconds(1);

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

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressType");
    return lit("undefined=%1").arg(static_cast<int>(type));
}

//-------------------------------------------------------------------------------------------------

TypedAddress::TypedAddress(HostAddress address_, AddressType type_):
    address(std::move(address_)),
    type(std::move(type_))
{
}

QString TypedAddress::toString() const
{
    return lm("%1(%2)").args(address, type);
}

//-------------------------------------------------------------------------------------------------

AddressAttribute::AddressAttribute(AddressAttributeType type_, quint64 value_):
    type(type_),
    value(value_)
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

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressAttributeType");
    return lit("undefined=%1").arg(static_cast<int>(type));
}

//-------------------------------------------------------------------------------------------------

AddressEntry::AddressEntry(AddressType type_, HostAddress host_):
    type(type_),
    host(std::move(host_))
{
}

AddressEntry::AddressEntry(const SocketAddress& address):
    type(AddressType::direct),
    host(address.address)
{
    attributes.push_back(AddressAttribute(
        AddressAttributeType::port, address.port));
}

bool AddressEntry::operator==(const AddressEntry& rhs) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;
}

bool AddressEntry::operator<(const AddressEntry& rhs) const
{
    return type < rhs.type && host < rhs.host && attributes < rhs.attributes;
}

QString AddressEntry::toString() const
{
    return lm("%1:%2(%3)").arg(type).arg(host).container(attributes);
}

//-------------------------------------------------------------------------------------------------

AddressResolver::AddressResolver():
    m_cloudAddressRegExp(QLatin1String(
        "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}"))
{
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const SocketAddress& endpoint)
{
    NX_ASSERT(!hostName.isIpAddress(), Q_FUNC_INFO, "Hostname should be unresolved");
    NX_ASSERT(endpoint.address.isIpAddress());
    NX_VERBOSE(this, lm("Added fixed address for %1: %2").args(hostName, endpoint));

    QnMutexLocker lk(&m_mutex);
    AddressEntry entry(endpoint);
    auto& entries = m_info.emplace(
        hostName,
        HostAddressInfo(isCloudHostName(&lk, hostName.toString())))
            .first->second.fixedEntries;

    const auto it = std::find(entries.begin(), entries.end(), entry);
    if (it == entries.end())
    {
        NX_LOGX(lm("New fixed address for %1: %2").args(hostName, endpoint), cl_logDEBUG1);
        entries.push_back(std::move(entry));

        // we possibly could also grabHandlers() to bring the result before DNS
        // is resolved (if in progress), but it does not cause a serious delay
        // so far
    }
}

void AddressResolver::removeFixedAddress(
    const HostAddress& hostName, boost::optional<SocketAddress> endpoint)
{
    NX_VERBOSE(this, lm("Removed fixed address for %1: %2").args(hostName, endpoint));

    QnMutexLocker lk(&m_mutex);
    const auto record = m_info.find(hostName);
    if (record == m_info.end())
        return;

    auto& entries = record->second.fixedEntries;
    if (endpoint)
    {
        AddressEntry entry(*endpoint);
        const auto it = std::find(entries.begin(), entries.end(), entry);
        if (it != entries.end())
        {
            NX_LOGX(lm("Removed fixed address for %1: %2").args(hostName, endpoint), cl_logDEBUG1);
            entries.erase(it);
        }
    }
    else
    {
        NX_LOGX(lm("Removed all fixed address for %1").args(hostName), cl_logDEBUG1);
        entries.clear();
    }
}

namespace {

// InPlaceResolver ---------------------------------------------------------------------------------
using ResolveHandler = AddressResolver::ResolveHandler;

class InPlaceResolver
{
public:
    InPlaceResolver(int ipVersion, const HostAddress& hostname);
    bool resolve(ResolveHandler* handler);
private:
    int m_ipVersion;
    HostAddress m_hostname;

    bool callHandler(HostAddress hostAddress, ResolveHandler* handler);
};

InPlaceResolver::InPlaceResolver(int ipVersion, const HostAddress& hostname):
    m_ipVersion(ipVersion),
    m_hostname(hostname)
{
    NX_ASSERT(m_ipVersion == AF_INET || m_ipVersion == AF_INET6);
    if (m_ipVersion != AF_INET && m_ipVersion != AF_INET6)
        NX_ERROR(this, "Unknown ip version");
}

bool InPlaceResolver::resolve(ResolveHandler* handler)
{
    if (m_hostname.isIpAddress())
        return callHandler(std::move(m_hostname), handler);

    if (m_ipVersion == AF_INET && m_hostname.ipV4())
        return callHandler(*m_hostname.ipV4(), handler);

    HostAddress::IpV6WithScope ipV6WithScope = m_hostname.ipV6();
    if (!ipV6WithScope.first || !m_hostname.isPureIpV6())
        return false;

    return callHandler(HostAddress(*ipV6WithScope.first, ipV6WithScope.second), handler);
}

bool InPlaceResolver::callHandler(HostAddress hostAddress, ResolveHandler* handler)
{
    NX_VERBOSE(this, lm("IP %1 resolved in place").arg(m_hostname));
    AddressEntry entry(AddressType::direct, std::move(hostAddress));
    (*handler)(SystemError::noError, std::deque<AddressEntry>({std::move(entry)}));

    return true;
}

static bool inPlaceResolve(int ipVersion, const HostAddress& hostAddress, ResolveHandler* handler)
{
    InPlaceResolver resolver(ipVersion, hostAddress);
    return resolver.resolve(handler);
}
// -------------------------------------------------------------------------------------------------

} // namespace

void AddressResolver::resolveAsync(
    const HostAddress& hostName,
    ResolveHandler handler,
    NatTraversalSupport natTraversalSupport,
    int ipVersion,
    void* requestId)
{
    if (inPlaceResolve(ipVersion, hostName, &handler))
        return;

    if (SocketGlobals::ini().isHostDisabled(hostName))
        return handler(SystemError::noPermission, std::deque<AddressEntry>());

    QnMutexLocker lk(&m_mutex);
    auto info = m_info.emplace(
        hostName,
        HostAddressInfo(isCloudHostName(&lk, hostName.toString()))).first;
    info->second.checkExpirations();
    tryFastDomainResolve(info);
    if (info->second.isResolved(natTraversalSupport))
    {
        auto entries = info->second.getAll();

        // TODO: #mux This is ugly fix for VMS-5777, should be properly fixed in 3.1.
        if (info->second.isLikelyCloudAddress && isMediatorAvailable())
        {
            const bool cloudAddressEntryPresent =
                std::count_if(
                    entries.begin(), entries.end(),
                    [](const AddressEntry& entry) { return entry.type == AddressType::cloud; }) > 0;
            if (!cloudAddressEntryPresent)
                entries.push_back(AddressEntry(AddressType::cloud, hostName));
        }

        lk.unlock();

        NX_VERBOSE(this, lm("Address %1 resolved from cache: %2").arg(hostName).container(entries));
        const auto code = entries.size() ? SystemError::noError : SystemError::hostNotFound;
        return handler(code, std::move(entries));
    }

    info->second.pendingRequests.insert(requestId);
    m_requests.insert(
        std::make_pair(
            requestId,
            RequestInfo(info->first, natTraversalSupport, std::move(handler))));

    NX_VERBOSE(this, lm("Address %1 will be resolved later by request %2").args(hostName, requestId));
    if (info->second.isLikelyCloudAddress && natTraversalSupport == NatTraversalSupport::enabled)
        mediatorResolve(info, &lk, true, ipVersion);
    else
        dnsResolve(info, &lk, natTraversalSupport == NatTraversalSupport::enabled, ipVersion);
}

std::deque<AddressEntry> AddressResolver::resolveSync(
    const HostAddress& hostName,
    NatTraversalSupport natTraversalSupport,
    int ipVersion)
{
    utils::promise<std::pair<SystemError::ErrorCode, std::deque<AddressEntry>>> promise;
    auto handler =
        [&](SystemError::ErrorCode code, std::deque<AddressEntry> entries)
        {
            promise.set_value({code, std::move(entries)});
        };

    resolveAsync(hostName, std::move(handler), natTraversalSupport, ipVersion);
    const auto result = promise.get_future().get();
    SystemError::setLastErrorCode(result.first);
    return result.second;
}

void AddressResolver::cancel(
    void* requestId, nx::utils::MoveOnlyFunc<void()> handler)
{
    boost::optional<utils::promise<bool>> promise;
    if (!handler)
    {
        // no handler means we have to wait for complete
        promise = utils::promise<bool>();
        handler = [&](){ promise->set_value(true); };
    }

    {
        utils::BarrierHandler barrier(std::move(handler));
        QnMutexLocker lk(&m_mutex);
        const auto range = m_requests.equal_range(requestId);
        for (auto it = range.first; it != range.second;)
        {
            NX_ASSERT(!it->second.guard, Q_FUNC_INFO,
                "Cancel has already been called for this requestId");

            if (it->second.inProgress && !it->second.guard)
                (it++)->second.guard = barrier.fork();
            else
                it = m_requests.erase(it);
        }
    }

    if (promise)
        promise->get_future().wait();
}

bool AddressResolver::isRequestIdKnown(void* requestId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_requests.count(requestId);
}

bool AddressResolver::isCloudHostName(const QString& hostName) const
{
    QnMutexLocker lk(&m_mutex);
    return isCloudHostName(&lk, hostName);
}

void AddressResolver::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    // TODO: make DnsResolver QnStoppableAsync
    m_dnsResolver.stop();
    handler();
}

bool AddressResolver::isValidForConnect(const SocketAddress& endpoint) const
{
    return (endpoint.port != 0) || isCloudHostName(endpoint.address.toString());
}

//-------------------------------------------------------------------------------------------------

AddressResolver::HostAddressInfo::HostAddressInfo(bool _isLikelyCloudAddress):
    isLikelyCloudAddress(_isLikelyCloudAddress),
    m_dnsState(State::unresolved),
    m_mediatorState(State::unresolved)
{
}

void AddressResolver::HostAddressInfo::setDnsEntries(
    std::vector<AddressEntry> entries)
{
    m_dnsState = State::resolved;
    m_dnsResolveTime = std::chrono::steady_clock::now();
    m_dnsEntries = std::move(entries);
}

void AddressResolver::HostAddressInfo::setMediatorEntries(
        std::vector<AddressEntry> entries)
{
    m_mediatorState = State::resolved;
    m_mediatorResolveTime = std::chrono::steady_clock::now();
    m_mediatorEntries = std::move(entries);
}

void AddressResolver::HostAddressInfo::checkExpirations()
{
    if (m_dnsState == State::resolved &&
        m_dnsResolveTime + kDnsCacheTimeout < std::chrono::steady_clock::now())
    {
        m_dnsState = State::unresolved;
        m_dnsEntries.clear();
    }

    if (m_mediatorState == State::resolved &&
        m_mediatorResolveTime + kMediatorCacheTimeout < std::chrono::steady_clock::now())
    {
        m_mediatorState = State::unresolved;
        m_mediatorEntries.clear();
    }
}

bool AddressResolver::HostAddressInfo::isResolved(
    NatTraversalSupport natTraversalSupport) const
{
    if (!fixedEntries.empty() || !m_dnsEntries.empty() || !m_mediatorEntries.empty())
        return true; // any address is better than nothing

    return (m_dnsState == State::resolved)
        && (natTraversalSupport == NatTraversalSupport::disabled
            || m_mediatorState == State::resolved);
}

std::deque<AddressEntry> AddressResolver::HostAddressInfo::getAll() const
{
    std::deque<AddressEntry> entries;
    const auto endeque =
        [&entries](const std::vector<AddressEntry>& v)
        {
            for (const auto i: v)
                entries.push_back(i);
        };

    endeque(fixedEntries);
    if (isLikelyCloudAddress)
    {
        endeque(m_mediatorEntries);
        endeque(m_dnsEntries);
    }
    else
    {
        endeque(m_dnsEntries);
        endeque(m_mediatorEntries);
    }

    return entries;
}

//-------------------------------------------------------------------------------------------------

AddressResolver::RequestInfo::RequestInfo(
    HostAddress address,
    NatTraversalSupport natTraversalSupport,
    ResolveHandler handler)
:
    address(std::move(address)),
    inProgress(false),
    natTraversalSupport(natTraversalSupport),
    handler(std::move(handler))
{
}

//-------------------------------------------------------------------------------------------------

bool AddressResolver::isMediatorAvailable() const
{
    return SocketGlobals::mediatorConnector().isConnected();
}

void AddressResolver::tryFastDomainResolve(HaInfoIterator info)
{
    const auto domain = info->first.toString();
    if (domain.indexOf(lit(".")) != -1)
        return; // only top level domains might be fast resolved

    iterateSubdomains(
        domain, [&](HaInfoIterator other)
        {
            info->second.fixedEntries = other->second.fixedEntries;
            return true; // just resolve to first avaliable
        });
}

void AddressResolver::dnsResolve(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator, int ipVersion)
{
    NX_LOGX(lm("dnsResolve. %1. %2").arg(info->first).arg((int)info->second.dnsState()), cl_logDEBUG2);

    switch (info->second.dnsState())
    {
        case HostAddressInfo::State::resolved:
            if (needMediator) mediatorResolve(info, lk, false, ipVersion);
            return;
        case HostAddressInfo::State::inProgress:
            return;
        case HostAddressInfo::State::unresolved:
            break; // continue
    }

    NX_LOGX(lm("dnsResolve async. %1").arg(info->first), cl_logDEBUG2);

    info->second.dnsProgress();
    QnMutexUnlocker ulk(lk);
    m_dnsResolver.resolveAsync(
        info->first.toString(),
        [this, info, needMediator, ipVersion](
            SystemError::ErrorCode code, std::deque<HostAddress> ips)
        {
            NX_LOGX(lm("dnsResolve async done. %1, %2").args(code, ips.size()), cl_logDEBUG2);

            std::vector<Guard> guards;

            QnMutexLocker lk(&m_mutex);
            std::vector<AddressEntry> entries;
            while (!ips.empty())
            {
                entries.emplace_back(AddressType::direct, std::move(ips.front()));
                ips.pop_front();
            }

            NX_VERBOSE(this, lm("Address %1 is resolved by DNS to %2")
                .arg(info->first).container(entries));

            info->second.setDnsEntries(std::move(entries));
            guards = grabHandlers(code, info);
            NX_LOGX(lm("dnsResolve async done. grabndlers.size() = %1").arg(guards.size()), cl_logDEBUG2);
            if (needMediator && !info->second.isResolved(NatTraversalSupport::enabled))
                mediatorResolve(info, &lk, false, ipVersion); // in case it's not resolved yet
        },
        ipVersion,
        this);
}

void AddressResolver::mediatorResolve(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needDns, int ipVersion)
{
    switch (info->second.mediatorState())
    {
        case HostAddressInfo::State::resolved:
            if (needDns) dnsResolve(info, lk, false, ipVersion);
            return;
        case HostAddressInfo::State::inProgress:
            return;
        case HostAddressInfo::State::unresolved:
            break; // continue
    }

    SystemError::ErrorCode resolveResult = SystemError::notImplemented;
    if (info->second.isLikelyCloudAddress && isMediatorAvailable())
    {
        info->second.setMediatorEntries({AddressEntry(AddressType::cloud, info->first)});
        resolveResult = SystemError::noError;
    }
    else
    {
        info->second.setMediatorEntries();
        resolveResult = SystemError::hostNotFound;
    }

    const auto unlockedGuard = makeScopeGuard(
        [lk, guards = grabHandlers(resolveResult, info)]() mutable
        {
            QnMutexUnlocker ulk(lk);
            guards.clear();
        });

    if (needDns && !info->second.isResolved(NatTraversalSupport::enabled))
        return dnsResolve(info, lk, false, ipVersion);
}

std::vector<Guard> AddressResolver::grabHandlers(
    SystemError::ErrorCode lastErrorCode,
    HaInfoIterator info)
{
    std::vector<Guard> guards;

    auto entries = info->second.getAll();
    for (auto req = info->second.pendingRequests.begin();
         req != info->second.pendingRequests.end();)
    {
        const auto range = m_requests.equal_range(*req);
        bool noPending = (range.first == range.second);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.address != info->first ||
                it->second.inProgress ||
                !info->second.isResolved(it->second.natTraversalSupport))
            {
                noPending = false;
                continue;
            }

            it->second.inProgress = true;
            guards.push_back(Guard(
                [this, it, entries, lastErrorCode, info]()
                {
                    Guard guard; //< Shall fire out of mutex scope
                    auto code = entries.empty() ? lastErrorCode : SystemError::noError;
                    it->second.handler(code, std::move(entries));

                    QnMutexLocker lk(&m_mutex);
                    NX_VERBOSE(this, lm("Address %1 is resolved by request %2 to %3")
                        .args(info->first, it->first).container(entries));

                    guard = std::move(it->second.guard);
                    m_requests.erase(it);
                    m_condition.wakeAll();
                }));
        }

        if(noPending)
            req = info->second.pendingRequests.erase(req);
        else
            ++req;
    }

    if (guards.size())
    {
        NX_VERBOSE(this, lm("There is(are) %1 about to be notified: %2 is resolved to %3")
            .args(guards.size(), info->first).container(entries));
    }

    return guards;
}

bool AddressResolver::isCloudHostName(
    QnMutexLockerBase* const /*lk*/,
    const QString& hostName) const
{
    return m_cloudAddressRegExp.exactMatch(hostName);
}

} // namespace cloud
} // namespace network
} // namespace nx
