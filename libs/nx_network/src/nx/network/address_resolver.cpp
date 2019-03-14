#include "address_resolver.h"

#include <nx/network/socket_global.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>

#include "cloud/cloud_address_resolver.h"
#include "resolve/predefined_host_resolver.h"
#include "resolve/text_ip_address_resolver.h"

static const auto kDnsCacheTimeout = std::chrono::seconds(10);
static const auto kMediatorCacheTimeout = std::chrono::seconds(1);

namespace nx {
namespace network {

AddressResolver::AddressResolver():
    m_cloudAddressRegExp(QLatin1String(
        "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}"))
{
    m_nonBlockingResolvers.push_back(std::make_unique<TextIpAddressResolver>());

    auto predefinedHostResolver = std::make_unique<PredefinedHostResolver>();
    m_predefinedHostResolver = predefinedHostResolver.get();
    m_nonBlockingResolvers.push_back(std::move(predefinedHostResolver));

    m_nonBlockingResolvers.push_back(std::make_unique<CloudAddressResolver>());
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const SocketAddress& endpoint)
{
    NX_ASSERT(!hostName.isIpAddress(), "Hostname should be unresolved");
    NX_ASSERT(endpoint.address.isIpAddress());
    NX_VERBOSE(this, lm("Added fixed address for %1: %2").args(hostName, endpoint));

    const AddressEntry entry(endpoint);
    m_predefinedHostResolver->addMapping(hostName.toStdString(), {endpoint});
}

void AddressResolver::removeFixedAddress(
    const HostAddress& hostName, boost::optional<SocketAddress> endpoint)
{
    NX_VERBOSE(this, lm("Removed fixed address for %1: %2").args(hostName, endpoint));

    if (endpoint)
        m_predefinedHostResolver->removeMapping(hostName.toStdString(), {*endpoint});
    else
        m_predefinedHostResolver->removeMapping(hostName.toStdString());
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
        std::deque<AddressEntry> result;
        result.push_back(AddressEntry(AddressType::direct, hostName));
        return handler(SystemError::noError, std::move(result));
    }

    if (SocketGlobals::instance().isHostBlocked(hostName))
        return handler(SystemError::noPermission, std::deque<AddressEntry>());

    std::deque<AddressEntry> resolvedAddresses;
    if (resolveNonBlocking(hostName.toString(), natTraversalSupport, ipVersion, &resolvedAddresses))
        return handler(SystemError::noError, std::move(resolvedAddresses));

    QnMutexLocker lk(&m_mutex);
    auto info = m_info.emplace(
        std::make_tuple(ipVersion, hostName),
        HostAddressInfo(isCloudHostName(&lk, hostName.toString()))).first;
    info->second.checkExpirations();
    if (info->second.isResolved(natTraversalSupport))
    {
        auto entries = info->second.getAll();

        // TODO: #mux This is ugly fix for VMS-5777, should be properly fixed in 3.1.
        if (info->second.isLikelyCloudAddress)
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
            RequestInfo(std::get<1>(info->first), natTraversalSupport, std::move(handler))));

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
            NX_ASSERT(!it->second.guard, "Cancel has already been called for this requestId");
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

bool AddressResolver::resolveNonBlocking(
    const QString& hostName,
    NatTraversalSupport natTraversalSupport,
    int ipVersion,
    std::deque<AddressEntry>* resolvedAddresses)
{
    for (const auto& nonBlockingResolver: m_nonBlockingResolvers)
    {
        // TODO: Remove following if.
        if ((natTraversalSupport == NatTraversalSupport::disabled || !m_isCloudResolveEnabled) &&
            dynamic_cast<CloudAddressResolver*>(nonBlockingResolver.get()) != nullptr)
        {
            continue;
        }

        const SystemError::ErrorCode resolveResult =
            nonBlockingResolver->resolve(hostName, ipVersion, resolvedAddresses);
        if (resolveResult == SystemError::noError)
            return true;
    }

    return false;
}

void AddressResolver::dnsResolve(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator, int ipVersion)
{
    NX_VERBOSE(this, lm("dnsResolve. %1. %2")
        .arg(std::get<1>(info->first)).arg((int)info->second.dnsState()));

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

    NX_VERBOSE(this, lm("dnsResolve async. %1").arg(std::get<1>(info->first)));

    info->second.dnsProgress();
    QnMutexUnlocker ulk(lk);
    m_dnsResolver.resolveAsync(
        std::get<1>(info->first).toString(),
        [this, info, needMediator, ipVersion](
            SystemError::ErrorCode code, std::deque<HostAddress> ips)
        {
            NX_VERBOSE(this, lm("dnsResolve async done. %1, %2").args(code, ips.size()));

            std::vector<nx::utils::Guard> guards;

            QnMutexLocker lk(&m_mutex);
            std::vector<AddressEntry> entries;
            while (!ips.empty())
            {
                entries.emplace_back(AddressType::direct, std::move(ips.front()));
                ips.pop_front();
            }

            NX_VERBOSE(this, lm("Address %1 is resolved by DNS to %2")
                .arg(std::get<1>(info->first)).container(entries));

            info->second.setDnsEntries(std::move(entries));
            guards = grabHandlers(code, info);
            NX_VERBOSE(this, lm("dnsResolve async done. grabndlers.size() = %1").arg(guards.size()));
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
    if (info->second.isLikelyCloudAddress)
    {
        info->second.setMediatorEntries(
            {AddressEntry(AddressType::cloud, std::get<1>(info->first))});
        resolveResult = SystemError::noError;
    }
    else
    {
        info->second.setMediatorEntries();
        resolveResult = SystemError::hostNotFound;
    }

    const auto unlockedGuard = nx::utils::makeScopeGuard(
        [lk, guards = grabHandlers(resolveResult, info)]() mutable
        {
            QnMutexUnlocker ulk(lk);
            guards.clear();
        });

    if (needDns && !info->second.isResolved(NatTraversalSupport::enabled))
        return dnsResolve(info, lk, false, ipVersion);
}

std::vector<nx::utils::Guard> AddressResolver::grabHandlers(
    SystemError::ErrorCode lastErrorCode,
    HaInfoIterator info)
{
    std::vector<nx::utils::Guard> guards;

    auto entries = info->second.getAll();
    for (auto req = info->second.pendingRequests.begin();
         req != info->second.pendingRequests.end();)
    {
        const auto range = m_requests.equal_range(*req);
        bool noPending = (range.first == range.second);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.address != std::get<1>(info->first) ||
                it->second.inProgress ||
                !info->second.isResolved(it->second.natTraversalSupport))
            {
                noPending = false;
                continue;
            }

            it->second.inProgress = true;
            guards.push_back(nx::utils::Guard(
                [this, it, entries, lastErrorCode, info]()
                {
                    nx::utils::Guard guard; //< Shall fire out of mutex scope
                    auto code = entries.empty() ? lastErrorCode : SystemError::noError;
                    it->second.handler(code, std::move(entries));

                    QnMutexLocker lk(&m_mutex);
                    NX_VERBOSE(this, lm("Address %1 is resolved by request %2 to %3")
                        .args(std::get<1>(info->first), it->first).container(entries));

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
            .args(guards.size(), std::get<1>(info->first)).container(entries));
    }

    return guards;
}

bool AddressResolver::isCloudHostName(
    QnMutexLockerBase* const /*lk*/,
    const QString& hostName) const
{
    return m_cloudAddressRegExp.exactMatch(hostName);
}

} // namespace network
} // namespace nx
