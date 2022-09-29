// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dns_resolver.h"

#ifdef Q_OS_UNIX
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
#endif

#include <algorithm>
#include <atomic>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/time.h>
#include <nx/utils/scope_guard.h>

#include "resolve/predefined_host_resolver.h"
#include "resolve/system_resolver.h"

namespace nx {
namespace network {

//-------------------------------------------------------------------------------------------------
// class DnsResolver::ResolveTask

DnsResolver::ResolveTask::ResolveTask(
    std::string hostAddress, Handler handler, RequestId requestId,
    size_t sequence, int ipVersion)
:
    hostAddress(std::move(hostAddress)),
    completionHandler(std::move(handler)),
    requestId(requestId),
    sequence(sequence),
    ipVersion(ipVersion)
{
    creationTime = nx::utils::monotonicTime();
}

//-------------------------------------------------------------------------------------------------
// class DnsResolver

static constexpr int kResolveThreadCount = 4;
static constexpr auto kResolveCacheValueLifetime = std::chrono::minutes(1);
static constexpr int kResolveCacheMaxSize = 1000;

DnsResolver::DnsResolver():
    m_resolveCache(
        kResolveCacheValueLifetime,
        kResolveCacheMaxSize,
        /*do not prolong value lifetime on usage*/ false)
{
    auto predefinedHostResolver = std::make_unique<PredefinedHostResolver>();
    m_predefinedHostResolver = predefinedHostResolver.get();
    registerResolver(std::move(predefinedHostResolver), 1);
    registerResolver(std::make_unique<SystemResolver>(), 0);

    for (int i = 0; i < kResolveThreadCount; ++i)
        m_resolveThreads.push_back(std::thread([this]() { resolveThreadMain(); }));

    // This thread reports the results of queries fulfilled by the cache only.
    // A separate thread is needed so that such reports are not blocked by potentially long DNS resolve.
    m_reportCachedResultThread = std::thread([this]() { reportCachedResultThreadMain(); });
}

DnsResolver::~DnsResolver()
{
    stop();
}

void DnsResolver::stop()
{
    NX_VERBOSE(this, "stop");

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_terminated)
            return;

        m_terminated = true;
        m_cond.wakeAll();
    }

    for (auto& t: m_resolveThreads)
        t.join();

    m_reportCachedResultThread.join();
}

std::chrono::milliseconds DnsResolver::resolveTimeout() const
{
    return m_resolveTimeout;
}

void DnsResolver::setResolveTimeout(std::chrono::milliseconds value)
{
    m_resolveTimeout = value;
}

void DnsResolver::resolveAsync(
    const std::string& hostname, Handler handler, int ipVersion, RequestId requestId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (auto val = m_resolveCache.getValue(std::make_tuple(ipVersion, hostname)); val)
    {
        m_cachedResults.push({std::move(handler), *val});
        return;
    }

    const auto seq = ++m_currentSequence;
    m_tasks.emplace(seq, ResolveTask(
        hostname, std::move(handler), requestId, seq, ipVersion));
    m_taskQueue.push_back(seq);

    m_cond.wakeAll();
}

SystemError::ErrorCode DnsResolver::resolveSync(
    const std::string& hostname,
    int ipVersion,
    ResolveResult* resolveResult)
{
    if (m_blockedHosts.count(hostname) > 0)
        return SystemError::hostNotFound;

    for (const auto& resolver: m_resolversByPriority)
    {
        ResolveResult localResolveResult;
        const auto resultCode = resolver.second->resolve(
            hostname, ipVersion, &localResolveResult);
        if (resultCode == SystemError::noError)
        {
            NX_ASSERT(!localResolveResult.entries.empty());
            *resolveResult = std::move(localResolveResult);
            return resultCode;
        }
    }

    return SystemError::hostNotFound;
}

void DnsResolver::addEtcHost(const std::string& name, std::vector<HostAddress> addresses)
{
    std::deque<AddressEntry> entries;
    for (auto& address: addresses)
        entries.push_back({ AddressType::direct, address });

    m_predefinedHostResolver->replaceMapping(name, std::move(entries));
}

void DnsResolver::removeEtcHost(const std::string& name)
{
    m_predefinedHostResolver->removeMapping(name);
}

void DnsResolver::blockHost(const std::string& hostname)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_blockedHosts.insert(hostname);
}

void DnsResolver::unblockHost(const std::string& hostname)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_blockedHosts.erase(hostname);
}

void DnsResolver::registerResolver(std::unique_ptr<AbstractResolver> resolver, int priority)
{
    m_resolversByPriority.emplace(priority, std::move(resolver));
}

int DnsResolver::minRegisteredResolverPriority() const
{
    if (m_resolversByPriority.empty())
        return 0;

    return m_resolversByPriority.crbegin()->first;
}

int DnsResolver::maxRegisteredResolverPriority() const
{
    if (m_resolversByPriority.empty())
        return 0;

    return m_resolversByPriority.cbegin()->first;
}

void DnsResolver::resolveThreadMain()
{
    NX_VERBOSE(this, "%1. Entered", __func__);

    NX_MUTEX_LOCKER lock(&m_mutex);
    while (!m_terminated)
    {
        if (m_taskQueue.empty())
        {
            m_cond.wait(lock.mutex());
            continue;
        }

        const auto taskSequence = m_taskQueue.front();
        m_taskQueue.pop_front();

        auto taskIter = m_tasks.find(taskSequence);
        if (taskIter == m_tasks.end())
            continue; //< E.g., the task was cancelled.

        ResolveTask task = std::move(taskIter->second);
        lock.unlock();
        {
            SystemError::ErrorCode resultCode = SystemError::noError;
            ResolveResult resolveResult;
            if (isExpired(task))
                resultCode = SystemError::timedOut;
            else
                resultCode = resolveSync(task.hostAddress, task.ipVersion, &resolveResult);

            std::deque<HostAddress> resolvedAddresses;
            for (auto& entry: resolveResult.entries)
                resolvedAddresses.push_back(std::move(entry.host));

            lock.relock();

            if (task.requestId)
            {
                if (!m_tasks.contains(task.sequence))
                    continue; //< The task has been cancelled.

                m_runningTaskRequestIds.insert(task.requestId);
                m_tasks.erase(task.sequence);
            }

            if (resolveResult.ttl)
            {
                // Caching the result.
                // TODO: #akolesnikov Ignoring the ttl value for now for simplicity.
                m_resolveCache.put(
                    std::make_tuple(task.ipVersion, task.hostAddress),
                    resolvedAddresses);
            }

            // Unlocking the mutex before completion handler invocation.
            lock.unlock();

            task.completionHandler(resultCode, std::move(resolvedAddresses));
        }
        lock.relock();

        m_runningTaskRequestIds.erase(task.requestId);
        m_cond.wakeAll();
    }

    NX_VERBOSE(this, "%1. Exiting", __func__);
}

void DnsResolver::reportCachedResultThreadMain()
{
    static constexpr auto kTimeout = std::chrono::milliseconds(100);

    NX_VERBOSE(this, "%1. Entered", __func__);

    while (!m_terminated)
    {
        auto val = m_cachedResults.pop(kTimeout);
        if (!val)
            continue;

        auto& handler = std::get<0>(*val);
        auto& entries = std::get<1>(*val);
        handler(SystemError::noError, std::move(entries));
    }

    NX_VERBOSE(this, "%1. Exiting", __func__);
}

bool DnsResolver::isExpired(const ResolveTask& task) const
{
    return nx::utils::monotonicTime() >= task.creationTime + m_resolveTimeout;
}

} // namespace network
} // namespace nx
