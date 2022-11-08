// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "address_resolver.h"

#include <nx/network/socket_global.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>

#include "cloud/cloud_address_resolver.h"
#include "resolve/predefined_host_resolver.h"
#include "resolve/text_ip_address_resolver.h"

namespace nx::network {

AddressResolver::AddressResolver()
{
    m_nonBlockingResolvers.push_back(std::make_unique<TextIpAddressResolver>());

    auto predefinedHostResolver = std::make_unique<PredefinedHostResolver>();
    m_predefinedHostResolver = predefinedHostResolver.get();
    m_nonBlockingResolvers.push_back(std::move(predefinedHostResolver));

    auto cloudAddressResolver = std::make_unique<CloudAddressResolver>();
    m_cloudAddressResolver = cloudAddressResolver.get();
    m_nonBlockingResolvers.push_back(std::move(cloudAddressResolver));
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const HostAddress& target)
{
    addFixedAddress(hostName, SocketAddress(target, /*no port*/ 0));
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostname, const SocketAddress& endpoint)
{
    NX_ASSERT(!hostname.isIpAddress(), "Hostname should be unresolved");
    NX_ASSERT(endpoint.address.isIpAddress());
    NX_VERBOSE(this, nx::format("Added fixed address for %1: %2").args(hostname, endpoint));

    const AddressEntry entry(endpoint);
    m_predefinedHostResolver->addMapping(hostname.toString(), {endpoint});
}

void AddressResolver::removeFixedAddress(
    const HostAddress& hostname, std::optional<SocketAddress> endpoint)
{
    NX_VERBOSE(this, nx::format("Removed fixed address for %1: %2").args(hostname, endpoint));

    if (endpoint)
        m_predefinedHostResolver->removeMapping(hostname.toString(), {*endpoint});
    else
        m_predefinedHostResolver->removeMapping(hostname.toString());
}

void AddressResolver::resolveAsync(
    const HostAddress& hostAddr,
    ResolveHandler handler,
    NatTraversalSupport natTraversalSupport,
    int ipVersion,
    void* requestId)
{
    if (hostAddr.isIpAddress())
    {
        std::deque<AddressEntry> result;
        result.push_back(AddressEntry(AddressType::direct, hostAddr));
        return handler(SystemError::noError, std::move(result));
    }

    if (SocketGlobals::instance().isHostBlocked(hostAddr))
        return handler(SystemError::noPermission, std::deque<AddressEntry>());

    std::string hostname = hostAddr.toString();

    ResolveResult resolved;
    if (resolveNonBlocking(hostname, natTraversalSupport, ipVersion, &resolved))
        return handler(SystemError::noError, std::move(resolved.entries));

    // Cloud addresses are already resolved in resolveNonBlocking.
    // So, further resolving with DNS only.

    NX_MUTEX_LOCKER lk(&m_mutex);

    auto info = m_dnsCache.emplace(
        ResolveTaskKey{ipVersion, hostname},
        HostAddressInfo(m_dnsCacheTimeout)).first;
    info->second.checkExpirations();
    if (info->second.isResolved())
    {
        auto entries = info->second.getAll();

        lk.unlock();

        NX_VERBOSE(this, "Address %1 resolved from cache to %2", hostname, entries);
        const auto code = entries.size() ? SystemError::noError : SystemError::hostNotFound;
        return handler(code, std::move(entries));
    }

    info->second.pendingRequests.insert(requestId);
    m_requests.emplace(
        requestId,
        RequestInfo(info->first.hostname, std::move(handler)));

    if (info->second.dnsState() == HostAddressInfo::State::inProgress)
    {
        NX_VERBOSE(this, "Hostname %1 (request %2) resolve is already in progress", hostname, requestId);
        return;
    }

    NX_VERBOSE(this, "Resolving hostname %1 (request %2) via DNS", hostname, requestId);
    dnsResolve(info, &lk, ipVersion);
}

std::deque<AddressEntry> AddressResolver::resolveSync(
    const HostAddress& hostname,
    NatTraversalSupport natTraversalSupport,
    int ipVersion)
{
    utils::promise<std::pair<SystemError::ErrorCode, std::deque<AddressEntry>>> promise;
    auto handler =
        [&](SystemError::ErrorCode code, std::deque<AddressEntry> entries)
        {
            promise.set_value({code, std::move(entries)});
        };

    resolveAsync(hostname, std::move(handler), natTraversalSupport, ipVersion);
    const auto result = promise.get_future().get();
    SystemError::setLastErrorCode(result.first);
    return result.second;
}

void AddressResolver::cancel(
    void* requestId, nx::utils::MoveOnlyFunc<void()> handler)
{
    std::optional<utils::promise<bool>> promise;
    if (!handler)
    {
        // no handler means we have to wait for complete
        promise = utils::promise<bool>();
        handler = [&](){ promise->set_value(true); };
    }

    {
        utils::BarrierHandler barrier(std::move(handler));
        NX_MUTEX_LOCKER lk(&m_mutex);
        const auto range = m_requests.equal_range(requestId);
        for (auto it = range.first; it != range.second;)
        {
            NX_ASSERT(!it->second.guard, "Cancel has already been called for this requestId");
            if (it->second.handlerAboutToBeInvoked && !it->second.guard)
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
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_requests.count(requestId);
}

bool AddressResolver::isCloudHostname(const std::string_view& hostname) const
{
    return m_cloudAddressResolver->isCloudHostname(hostname);
}

bool AddressResolver::isCloudHostname(const QString& hostname) const
{
    return isCloudHostname(hostname.toStdString());
}

bool AddressResolver::isCloudHostname(const char* hostname) const
{
    return isCloudHostname(std::string_view(hostname));
}

void AddressResolver::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_dnsResolver.stop();
    handler();
}

void AddressResolver::setDnsCacheTimeout(std::chrono::milliseconds timeout)
{
    m_dnsCacheTimeout = timeout;
}

//-------------------------------------------------------------------------------------------------

AddressResolver::HostAddressInfo::HostAddressInfo(
    std::chrono::milliseconds dnsCacheTimeout)
    :
    m_dnsCacheTimeout(dnsCacheTimeout)
{
}

void AddressResolver::HostAddressInfo::setDnsEntries(
    std::deque<AddressEntry> entries)
{
    m_dnsState = State::resolved;
    m_dnsResolveTime = std::chrono::steady_clock::now();
    m_dnsEntries = std::move(entries);
}

void AddressResolver::HostAddressInfo::checkExpirations()
{
    if (m_dnsState == State::resolved &&
        m_dnsResolveTime + m_dnsCacheTimeout < std::chrono::steady_clock::now())
    {
        m_dnsState = State::unresolved;
        m_dnsEntries.reset();
    }
}

bool AddressResolver::HostAddressInfo::isResolved() const
{
    return m_dnsEntries != std::nullopt;
}

std::deque<AddressEntry> AddressResolver::HostAddressInfo::getAll() const
{
    return m_dnsEntries ? *m_dnsEntries : std::deque<AddressEntry>();
}

//-------------------------------------------------------------------------------------------------

std::string AddressResolver::ResolveTaskKey::toString() const
{
    return nx::format("%1 (%2)", hostname, ipVersion == AF_INET ? "v4" : "v6").toStdString();
}

bool AddressResolver::ResolveTaskKey::operator<(const ResolveTaskKey& rhs) const
{
    return std::tie(ipVersion, hostname) < std::tie(rhs.ipVersion, rhs.hostname);
}

//-------------------------------------------------------------------------------------------------

AddressResolver::RequestInfo::RequestInfo(
    std::string hostname,
    ResolveHandler handler)
:
    hostname(std::move(hostname)),
    handler(std::move(handler))
{
}

//-------------------------------------------------------------------------------------------------

bool AddressResolver::resolveNonBlocking(
    const std::string& hostname,
    NatTraversalSupport natTraversalSupport,
    int ipVersion,
    ResolveResult* resolved)
{
    for (const auto& nonBlockingResolver: m_nonBlockingResolvers)
    {
        // TODO: Remove following if.
        if (natTraversalSupport == NatTraversalSupport::disabled &&
            dynamic_cast<CloudAddressResolver*>(nonBlockingResolver.get()) != nullptr)
        {
            continue;
        }

        const SystemError::ErrorCode resolveResult =
            nonBlockingResolver->resolve(hostname, ipVersion, resolved);
        if (resolveResult == SystemError::noError)
            return true;
    }

    return false;
}

void AddressResolver::dnsResolve(
    HaInfoIterator info, nx::Locker<nx::Mutex>* lk, int ipVersion)
{
    NX_VERBOSE(this, "Starting async DNS resolve for %1", info->first);

    info->second.dnsProgress();
    nx::Unlocker<nx::Mutex> ulk(lk);

    m_dnsResolver.resolveAsync(
        info->first.hostname,
        [this, info](
            SystemError::ErrorCode code, std::deque<HostAddress> ips)
        {
            NX_VERBOSE(this, "Async DNS resolve completed: %1. Found entries: %2", code, ips);

            std::deque<AddressEntry> entries;
            for (auto& entry: ips)
                entries.emplace_back(AddressType::direct, std::move(entry));

            reportResult(info, code, std::move(entries));
        },
        ipVersion,
        this);
}

void AddressResolver::reportResult(
    HaInfoIterator info,
    SystemError::ErrorCode lastErrorCode,
    std::deque<AddressEntry> entries)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    auto requestsToCompleteIters = selectRequestsToComplete(lk, info);

    info->second.setDnsEntries(entries);

    NX_VERBOSE(this, "There is(are) %1 about to be notified: %2 is resolved to %3",
        requestsToCompleteIters.size(), info->first, entries);

    lk.unlock();

    lastErrorCode = entries.empty() ? lastErrorCode : SystemError::noError;
    for (const auto& it: requestsToCompleteIters)
        it->second.handler(lastErrorCode, entries);

    lk.relock();

    std::vector<nx::utils::Guard> guards;
    for (const auto& it: requestsToCompleteIters)
    {
        NX_VERBOSE(this, "Address %1 is resolved by request %2 to %3",
            info->first, it->first, entries);

        guards.push_back(std::move(it->second.guard));
        m_requests.erase(it);
    }

    lk.unlock();

    // guards MUST fire out of the mutex scope.
    for (auto& guard: guards)
        guard.fire();
}

std::vector<std::multimap<void*, AddressResolver::RequestInfo>::iterator>
    AddressResolver::selectRequestsToComplete(
        const nx::Locker<nx::Mutex>&,
        HaInfoIterator info)
{
    std::vector<std::multimap<void*, AddressResolver::RequestInfo>::iterator> result;

    for (auto reqIt = info->second.pendingRequests.begin();
        reqIt != info->second.pendingRequests.end();)
    {
        const auto range = m_requests.equal_range(*reqIt);
        bool noPending = (range.first == range.second);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.hostname != info->first.hostname ||
                it->second.handlerAboutToBeInvoked)
            {
                noPending = false;
                continue;
            }

            it->second.handlerAboutToBeInvoked = true;
            result.push_back(it);
        }

        if (noPending)
            reqIt = info->second.pendingRequests.erase(reqIt);
        else
            ++reqIt;
    }

    return result;
}

std::string_view AddressResolver::toString(HostAddressInfo::State state)
{
    switch (state)
    {
        case HostAddressInfo::State::unresolved:
            return "unresolved";
        case HostAddressInfo::State::resolved:
            return "resolved";
        case HostAddressInfo::State::inProgress:
            return "inProgress";
    }

    return "unknown";
}

} // namespace nx::network
