#include "address_resolver.h"

#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/fusion/serialization/lexical.h>

#define DEBUG_LOG(MESSAGE) do \
{ \
    if (nx::network::SocketGlobals::debugIni().addressResolver) \
        NX_LOGX(MESSAGE, cl_logDEBUG1); \
} while (0)

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

TypedAddress::TypedAddress(HostAddress address_, AddressType type_)
:
    address(std::move(address_)),
    type(std::move(type_))
{
}

QString TypedAddress::toString() const
{
    return lm("%1(%2)").args(address, type);
}

AddressAttribute::AddressAttribute(AddressAttributeType type_, quint64 value_)
    : type(type_)
    , value(value_)
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

AddressEntry::AddressEntry(AddressType type_, HostAddress host_)
    : type(type_)
    , host(std::move(host_))
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
    return lm("%1:%2(%3)").arg(type).arg(host).container(attributes);
}

AddressResolver::AddressResolver(
    std::unique_ptr<hpm::api::MediatorClientTcpConnection> mediatorConnection)
:
    m_mediatorConnection(std::move(mediatorConnection)),
    m_cloudAddressRegExp(QLatin1String(
        "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}"))
{
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const SocketAddress& endpoint)
{
    NX_ASSERT(!hostName.isIpAddress(), Q_FUNC_INFO, "Hostname should be unresolved");
    NX_ASSERT(endpoint.address.isIpAddress());
    DEBUG_LOG(lm("Added fixed address for %1: %2").args(hostName, endpoint));

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
    DEBUG_LOG(lm("Removed fixed address for %1: %2").args(hostName, endpoint));

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
            DEBUG_LOG(lm("Domain %1 resolution on mediator result: %2").args(domain, resultCode));
            std::vector<TypedAddress> result;
            {
                QnMutexLocker lk(&m_mutex);
                iterateSubdomains(
                    domain.toString(), [&](HaInfoIterator info)
                    {
                        result.emplace_back(info->first, AddressType::direct);
                        return false; // continue
                    });
            }

            for (const auto& host : response.hostNames)
            {
                HostAddress address(QString::fromUtf8(host));
                result.emplace_back(std::move(address), AddressType::cloud);
            }

            DEBUG_LOG(lm("Domain %1 is resolved to: %2").arg(domain).container(result));
            handler(std::move(result));
        });
}

void AddressResolver::resolveAsync(
    const HostAddress& hostName,
    ResolveHandler handler,
    NatTraversalSupport natTraversalSupport,
    int ipVersion,
    void* requestId)
{
    if (hostName.isIpAddress())
    {
        DEBUG_LOG(lm("IP %1 is already resolved").arg(hostName));
        AddressEntry entry(AddressType::direct, hostName);
        return handler(SystemError::noError, std::deque<AddressEntry>({std::move(entry)}));
    }

    // Checking if hostName is fixed address, to speed up resolution when IPv6 is disabled.
    if (ipVersion == AF_INET)
    {
        const auto hostStr = hostName.toString().toStdString();
        // TODO: #ak Use InetPton here on mswin2.
        const auto ipv4Address = inet_addr(hostStr.c_str());
        if (ipv4Address != INADDR_NONE)
        {
            // Resolved.
            DEBUG_LOG("Hostname %1 is IP v4 address");
            struct in_addr resolvedAddress;
            memset(&resolvedAddress, 0, sizeof(resolvedAddress));
            resolvedAddress.s_addr = ipv4Address;
            AddressEntry entry(AddressType::direct, HostAddress(resolvedAddress));
            return handler(SystemError::noError, std::deque<AddressEntry>({ std::move(entry) }));
        }
    }

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

        DEBUG_LOG(lm("Address %1 resolved from cache: %2").arg(hostName).container(entries));
        const auto code = entries.size() ? SystemError::noError : SystemError::hostNotFound;
        return handler(code, std::move(entries));
    }

    info->second.pendingRequests.insert(requestId);
    m_requests.insert(
        std::make_pair(
            requestId,
            RequestInfo(info->first, natTraversalSupport, std::move(handler))));

    DEBUG_LOG(lm("Address %1 will be resolved later by request %2").args(hostName, requestId));
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
    m_mediatorConnection->pleaseStop(
        [this, handler = std::move(handler)]()
        {
            m_mediatorConnection.reset();
            handler();
        });
}

bool AddressResolver::isValidForConnect(const SocketAddress& endpoint) const
{
    return (endpoint.port != 0) || isCloudHostName(endpoint.address.toString());
}

AddressResolver::HostAddressInfo::HostAddressInfo(bool _isLikelyCloudAddress)
:
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

bool AddressResolver::isMediatorAvailable() const
{
    return (bool) SocketGlobals::mediatorConnector().mediatorAddress();
}

void AddressResolver::tryFastDomainResolve(HaInfoIterator info)
{
    const auto domain = info->first.toString();
    if (domain.indexOf(lit(".")) != -1)
        return; // only top level domains might be fast resolved

    const bool hasFound = iterateSubdomains(
        domain, [&](HaInfoIterator other)
        {
            info->second.fixedEntries = other->second.fixedEntries;
            return true; // just resolve to first avaliable
        });

    if (!hasFound)
        info->second.fixedEntries.clear();
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

            DEBUG_LOG(lm("Address %1 is resolved by DNS to %2")
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

    if (kResolveOnMediator)
        return mediatorResolveImpl(info, lk, needDns, ipVersion);

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

void AddressResolver::mediatorResolveImpl(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needDns, int ipVersion)
{
    info->second.mediatorProgress();
    QnMutexUnlocker ulk(lk);
    m_mediatorConnection->resolvePeer(
        nx::hpm::api::ResolvePeerRequest(info->first.toString().toUtf8()),
        [this, info, needDns, ipVersion](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolvePeerResponse response)
        {
            std::vector<Guard> guards;

            QnMutexLocker lk(&m_mutex);
            std::vector<AddressEntry> entries;
            if (resultCode == nx::hpm::api::ResultCode::ok)
            {
                for (const auto& it : response.endpoints)
                {
                    AddressEntry entry(AddressType::direct, it.address);
                    entry.attributes.push_back(AddressAttribute(
                        AddressAttributeType::port, it.port));
                    entries.push_back(std::move(entry));
                }

                // if target host supports cloud connect, adding corresponding
                // address entry
                if (response.connectionMethods != 0)
                    entries.emplace_back(AddressType::cloud, info->first);
            }

            DEBUG_LOG(lm("Address %1 is resolved by mediator to %2")
                .arg(info->first).container(entries));

            const auto code = (resultCode == nx::hpm::api::ResultCode::ok)
                ? SystemError::noError
                : SystemError::hostUnreach; //TODO #ak correct error translation

            info->second.setMediatorEntries(std::move(entries));
            guards = grabHandlers(code, info);
            if (needDns && !info->second.isResolved(NatTraversalSupport::enabled))
                dnsResolve(info, &lk, false, ipVersion); // in case it's not resolved yet
        });
}

std::vector<Guard> AddressResolver::grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info)
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
                    DEBUG_LOG(lm("Address %1 is resolved by request %2 to %3")
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
        DEBUG_LOG(lm("There is(are) %1 about to be notified: %2 is resolved to %3")
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
