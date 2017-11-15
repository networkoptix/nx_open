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

namespace nx {
namespace network {

constexpr auto kDefaultDnsResolveTimeout = std::chrono::seconds(15);

//-------------------------------------------------------------------------------------------------
// class DnsResolver::ResolveTask

DnsResolver::ResolveTask::ResolveTask(
    QString hostAddress, Handler handler, RequestId requestId,
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

DnsResolver::DnsResolver():
    m_terminated(false),
    m_runningTaskRequestId(nullptr),
    m_currentSequence(0),
    m_resolveTimeout(kDefaultDnsResolveTimeout),
    m_predefinedHostResolver(nullptr)
{
    auto predefinedHostResolver = std::make_unique<PredefinedHostResolver>();
    m_predefinedHostResolver = predefinedHostResolver.get();
    registerResolver(std::move(predefinedHostResolver), 1);
    registerResolver(std::make_unique<SystemResolver>(), 0);

    start();
}

DnsResolver::~DnsResolver()
{
    stop();
}

void DnsResolver::pleaseStop()
{
    NX_LOGX(lm("DnsResolver::pleaseStop"), cl_logDEBUG2);

    QnMutexLocker lock(&m_mutex);
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
    const QString& hostName, Handler handler, int ipVersion, RequestId requestId)
{
    QnMutexLocker lock(&m_mutex);
    m_taskQueue.push_back(ResolveTask(
        hostName, std::move(handler), requestId, ++m_currentSequence, ipVersion));

    m_cond.wakeAll();
}

SystemError::ErrorCode DnsResolver::resolveSync(
    const QString& hostName,
    int ipVersion,
    std::deque<HostAddress>* resolvedAddresses)
{
    for (const auto& resolver: m_resolversByPriority)
    {
        const auto resultCode = resolver.second->resolve(hostName, ipVersion, resolvedAddresses);
        if (resultCode == SystemError::noError)
        {
            NX_ASSERT(!resolvedAddresses->empty());
            return resultCode;
        }
    }

    return SystemError::hostNotFound;
}

void DnsResolver::cancel(RequestId requestId, bool waitForRunningHandlerCompletion)
{
    QnMutexLocker lock( &m_mutex );
    // TODO: #ak improve search complexity
    m_taskQueue.remove_if(
        [requestId](const ResolveTask& task){ return task.requestId == requestId; } );

    // We are waiting only for handler completion but not resolveSync completion.
    while (waitForRunningHandlerCompletion && (m_runningTaskRequestId == requestId))
        m_cond.wait(&m_mutex);
}

bool DnsResolver::isRequestIdKnown(RequestId requestId) const
{
    QnMutexLocker lock( &m_mutex );
    //TODO #ak improve search complexity
    return m_runningTaskRequestId == requestId ||
        std::find_if(
            m_taskQueue.cbegin(), m_taskQueue.cend(),
            [requestId](const ResolveTask& task) { return task.requestId == requestId; }
        ) != m_taskQueue.cend();
}

void DnsResolver::addEtcHost(const QString& name, std::vector<HostAddress> addresses)
{
    m_predefinedHostResolver->addEtcHost(name, std::move(addresses));
}

void DnsResolver::removeEtcHost(const QString& name)
{
    m_predefinedHostResolver->removeEtcHost(name);
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
    NX_LOGX(lm("DnsResolver::run. Entered"), cl_logDEBUG2);

    QnMutexLocker lock(&m_mutex);
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

    NX_LOGX(lm("DnsResolver::run. Exiting. %1").arg(m_terminated), cl_logDEBUG2);
}

bool DnsResolver::isExpired(const ResolveTask& task) const
{
    return nx::utils::monotonicTime() >= task.creationTime + m_resolveTimeout;
}

} // namespace network
} // namespace nx
