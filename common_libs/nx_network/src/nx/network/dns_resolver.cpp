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

#include <utils/common/guard.h>

namespace nx {
namespace network {

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
}

DnsResolver::DnsResolver()
:
    m_terminated(false),
    m_runningTaskRequestId(nullptr),
    m_currentSequence(0)
{
    start();
}

DnsResolver::~DnsResolver()
{
    stop();
}

void DnsResolver::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
}

void DnsResolver::resolveAsync(
    const QString& hostName, Handler handler, int ipVersion, RequestId requestId)
{
    QnMutexLocker lk(&m_mutex);
    m_taskdeque.push_back(ResolveTask(
        hostName, std::move(handler), requestId, ++m_currentSequence, ipVersion));

    m_cond.wakeAll();
}

static std::deque<HostAddress> convertAddrInfo(addrinfo* addressInfo)
{
    std::deque<HostAddress> ipAddresses;
    for (addrinfo* info = addressInfo; info; info = info->ai_next)
    {
        HostAddress newAddress;
        switch (info->ai_family)
        {
            case AF_INET:
                newAddress = HostAddress(((sockaddr_in*)(info->ai_addr))->sin_addr);
                break;

            case AF_INET6:
                newAddress = HostAddress(((sockaddr_in6*)(info->ai_addr))->sin6_addr);
                break;

            default:
                continue;
        }

        // There are may be more than one service on a single node, no reason to provide
        // duplicates as we do not provide any extra information about the node.
        if (std::find(ipAddresses.begin(), ipAddresses.end(), newAddress) == ipAddresses.end())
            ipAddresses.push_back(std::move(newAddress));
    }

    if (ipAddresses.empty())
        SystemError::setLastErrorCode(SystemError::hostNotFound);

    return std::move(ipAddresses);
}

std::deque<HostAddress> DnsResolver::resolveSync(const QString& hostName, int ipVersion)
{
    auto resultCode = SystemError::noError;
    const auto guard = makeScopedGuard([&](){ SystemError::setLastErrorCode(resultCode); });
    if (hostName.isEmpty())
    {
        resultCode = SystemError::invalidData;
        return std::deque<HostAddress>();
    }

    std::deque<HostAddress> ipAddresses = getEtcHost(hostName, ipVersion);
    if (!ipAddresses.empty())
        return ipAddresses;

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_ALL;    /* For wildcard IP address */

    // Do not serach for IPv6 addresseses on IpV4 sockets
    if (ipVersion == AF_INET)
        hints.ai_family = ipVersion;

    addrinfo* addressInfo = nullptr;
    int status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);

    if (status == EAI_BADFLAGS)
    {
        // if the lookup failed with AI_ALL, try again without it
        hints.ai_flags = 0;
        status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);
    }

    if (status != 0)
    {
        switch (status)
        {
            case EAI_NONAME: resultCode = SystemError::hostNotFound; break;
            case EAI_AGAIN: resultCode = SystemError::again; break;
            case EAI_MEMORY: resultCode = SystemError::nomem; break;

            #ifdef __linux__
                case EAI_SYSTEM: resultCode = SystemError::getLastOSErrorCode(); break;
            #endif

            // TODO: #mux Translate some other status codes?
            default: resultCode = SystemError::dnsServerFailure; break;
        };
        
        return std::deque<HostAddress>();
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addressInfoGuard(addressInfo, &freeaddrinfo);
    const auto result = convertAddrInfo(addressInfo);
    if (result.empty())
        resultCode = SystemError::hostNotFound;
    
    return result;
}

void DnsResolver::cancel(RequestId requestId, bool waitForRunningHandlerCompletion)
{
    QnMutexLocker lk( &m_mutex );
    // TODO: #ak improve search complexity
    m_taskdeque.remove_if(
        [requestId](const ResolveTask& task){ return task.requestId == requestId; } );

    // We are waiting only for handler completion but not resolveSync completion.
    while(waitForRunningHandlerCompletion && (m_runningTaskRequestId == requestId))
        m_cond.wait(&m_mutex);
}

bool DnsResolver::isRequestIdKnown(RequestId requestId) const
{
    QnMutexLocker lk( &m_mutex );
    //TODO #ak improve search complexity
    return m_runningTaskRequestId == requestId ||
        std::find_if(
            m_taskdeque.cbegin(), m_taskdeque.cend(),
            [requestId](const ResolveTask& task) { return task.requestId == requestId; }
        ) != m_taskdeque.cend();
}


void DnsResolver::addEtcHost(const QString& name, std::vector<HostAddress> addresses)
{
    QnMutexLocker lk(&m_ectHostsMutex);
    m_etcHosts[name] = std::move(addresses);
}

void DnsResolver::removeEtcHost(const QString& name)
{
    QnMutexLocker lk(&m_ectHostsMutex);
    m_etcHosts.erase(name);
}

std::deque<HostAddress> DnsResolver::getEtcHost(const QString& name, int ipVersion)
{
    QnMutexLocker lk(&m_ectHostsMutex);
    const auto it = m_etcHosts.find(name);
    if (it == m_etcHosts.end())
        return std::deque<HostAddress>();

    std::deque<HostAddress> ipAddresses;
    for (const auto address: it->second)
    {
        if (ipVersion != AF_INET || address.ipV4())
             ipAddresses.push_back(address);
    }

    return ipAddresses;
}

void DnsResolver::run()
{
    QnMutexLocker lk(&m_mutex);
    while(!m_terminated)
    {
        while (m_taskdeque.empty() && !m_terminated)
            m_cond.wait(&m_mutex);

        if (m_terminated)
            break;  //not completing posted tasks

        ResolveTask task = std::move(m_taskdeque.front());
        lk.unlock();
        {
            auto result = resolveSync(task.hostAddress, task.ipVersion);
            auto code = result.empty() ? SystemError::getLastOSErrorCode() : SystemError::noError;
            if (task.requestId)
            {
                lk.relock();
                if (m_taskdeque.empty() || (m_taskdeque.front().sequence != task.sequence))
                    continue;   //current task has been cancelled

                m_runningTaskRequestId = task.requestId;
                m_taskdeque.pop_front();
                lk.unlock();
            }

            task.completionHandler(code, result);
        }
        lk.relock();

        m_runningTaskRequestId = nullptr;
        m_cond.wakeAll();
    }
}

} // namespace network
} // namespace nx
