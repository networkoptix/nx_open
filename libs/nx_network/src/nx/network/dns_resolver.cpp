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

DnsResolver::DnsResolver()
{
    auto predefinedHostResolver = std::make_unique<PredefinedHostResolver>();
    m_predefinedHostResolver = predefinedHostResolver.get();
    registerResolver(std::move(predefinedHostResolver), 1);
    registerResolver(std::make_unique<SystemResolver>(), 0);
    setObjectName("DnsResolver");
    start();
}

DnsResolver::~DnsResolver()
{
    stop();
}

void DnsResolver::pleaseStop()
{
    NX_VERBOSE(this, "pleaseStop");

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
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
    const std::string& hostName, Handler handler, int ipVersion, RequestId requestId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_taskQueue.push_back(ResolveTask(
        hostName, std::move(handler), requestId, ++m_currentSequence, ipVersion));

    m_cond.wakeAll();
}

SystemError::ErrorCode DnsResolver::resolveSync(
    const std::string& hostName,
    int ipVersion,
    std::deque<HostAddress>* resolvedAddresses)
{
    if (m_blockedHosts.count(hostName) > 0)
        return SystemError::hostNotFound;

    for (const auto& resolver: m_resolversByPriority)
    {
        std::deque<AddressEntry> resolvedAddressesEntries;
        const auto resultCode = resolver.second->resolve(
            hostName, ipVersion, &resolvedAddressesEntries);
        if (resultCode == SystemError::noError)
        {
            NX_ASSERT(!resolvedAddressesEntries.empty());
            for (auto& entry: resolvedAddressesEntries)
                resolvedAddresses->push_back(std::move(entry.host));
            return resultCode;
        }
    }

    return SystemError::hostNotFound;
}

void DnsResolver::cancel(RequestId requestId, bool waitForRunningHandlerCompletion)
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    // TODO: #akolesnikov improve search complexity
    m_taskQueue.remove_if(
        [requestId](const ResolveTask& task){ return task.requestId == requestId; } );

    // We are waiting only for handler completion but not resolveSync completion.
    while (waitForRunningHandlerCompletion && (m_runningTaskRequestId == requestId))
        m_cond.wait(&m_mutex);
}

bool DnsResolver::isRequestIdKnown(RequestId requestId) const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    //TODO #akolesnikov improve search complexity
    return m_runningTaskRequestId == requestId ||
        std::find_if(
            m_taskQueue.cbegin(), m_taskQueue.cend(),
            [requestId](const ResolveTask& task) { return task.requestId == requestId; }
        ) != m_taskQueue.cend();
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

void DnsResolver::blockHost(const std::string& hostName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_blockedHosts.insert(hostName);
}

void DnsResolver::unblockHost(const std::string& hostName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_blockedHosts.erase(hostName);
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

void DnsResolver::run()
{
    NX_VERBOSE(this, nx::format("run. Entered"));

    NX_MUTEX_LOCKER lock(&m_mutex);
    while (!m_terminated)
    {
        while (m_taskQueue.empty() && !m_terminated)
            m_cond.wait(lock.mutex());

        if (m_terminated)
            break;  //not completing posted tasks

        ResolveTask task = std::move(m_taskQueue.front());
        lock.unlock();
        {
            SystemError::ErrorCode resultCode = SystemError::noError;
            std::deque<HostAddress> resolvedAddresses;
            if (isExpired(task))
                resultCode = SystemError::timedOut;
            else
                resultCode = resolveSync(task.hostAddress, task.ipVersion, &resolvedAddresses);

            if (task.requestId)
            {
                lock.relock();
                if (m_taskQueue.empty() || (m_taskQueue.front().sequence != task.sequence))
                    continue;   //current task has been cancelled

                m_runningTaskRequestId = task.requestId;
                m_taskQueue.pop_front();
                lock.unlock();
            }

            task.completionHandler(resultCode, std::move(resolvedAddresses));
        }
        lock.relock();

        m_runningTaskRequestId = nullptr;
        m_cond.wakeAll();
    }

    NX_VERBOSE(this, nx::format("run. Exiting. %1").arg(m_terminated));
}

bool DnsResolver::isExpired(const ResolveTask& task) const
{
    return nx::utils::monotonicTime() >= task.creationTime + m_resolveTimeout;
}

} // namespace network
} // namespace nx
