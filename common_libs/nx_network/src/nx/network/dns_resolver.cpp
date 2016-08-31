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

namespace nx {
namespace network {

DnsResolver::ResolveTask::ResolveTask(
    HostAddress _hostAddress,
    std::function<void(SystemError::ErrorCode, const HostAddress&)> _completionHandler,
    RequestID _reqID,
    size_t _sequence,
    int _ipVersion)
:
    hostAddress( std::move( _hostAddress ) ),
    completionHandler( _completionHandler ),
    reqID( _reqID ),
    sequence( _sequence ),
    ipVersion(_ipVersion)
{
}

DnsResolver::DnsResolver()
:
    m_terminated( false ),
    m_runningTaskReqID( nullptr ),
    m_currentSequence( 0 )
{
    start();
}

DnsResolver::~DnsResolver()
{
    stop();
}

void DnsResolver::pleaseStop()
{
    QnMutexLocker lk( &m_mutex );
    m_terminated = true;
    m_cond.wakeAll();
}

void DnsResolver::resolveAddressAsync(
    const HostAddress& addressToResolve,
    std::function<void (SystemError::ErrorCode, const HostAddress&)>&& completionHandler,
    int ipVersion, RequestID reqID )
{
    QnMutexLocker lk( &m_mutex );
    m_taskQueue.push_back( ResolveTask(
        addressToResolve, completionHandler, reqID, ++m_currentSequence, ipVersion ) );

    m_cond.wakeAll();
}

bool DnsResolver::resolveAddressSync(
    const QString& hostName, HostAddress* const resolvedAddress, int ipVersion )
{
    if( hostName.isEmpty() )
    {
        SystemError::setLastErrorCode( SystemError::hostNotFound );
        return false;
    }

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
        SystemError::ErrorCode code;
        switch (status)
        {
            case EAI_NONAME: code = SystemError::hostNotFound; break;
            case EAI_AGAIN: code = SystemError::again; break;
            case EAI_MEMORY: code = SystemError::nomem; break;
#ifdef __linux__
            case EAI_SYSTEM: return false; // System error returned in `errno'
#endif

            // TODO: #mux Translate some other status codes?
            default: code = SystemError::dnsServerFailure; break;
        };

        SystemError::setLastErrorCode( code );
        return false;
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addressInfoGuard(addressInfo, &freeaddrinfo);

    std::vector<HostAddress> ipAddresses;
    for (addrinfo* info = addressInfo; info; info = info->ai_next)
    {
        if (info->ai_family == AF_INET)
        {
            ipAddresses.emplace_back(HostAddress(
                ((sockaddr_in*)(info->ai_addr))->sin_addr));
        }
        else if (info->ai_family == AF_INET6)
        {
            ipAddresses.emplace_back(HostAddress(
                ((sockaddr_in6*)(info->ai_addr))->sin6_addr));
        }
    }

    if (ipAddresses.empty())
    {
        SystemError::setLastErrorCode( SystemError::hostNotFound );
        return true;
    }

    // TODO: support multi-address!
    //  currently we return first v4 address (if any) or first v6
    std::sort(ipAddresses.begin(), ipAddresses.end(),
        [](const HostAddress& left, const HostAddress& right)
    {
        // The address with IPv4 comes before address without IPv4
        return (left.m_ipV4) && (!right.m_ipV4);
    });

    resolvedAddress->m_ipV4 = std::move(ipAddresses.begin()->m_ipV4);
    resolvedAddress->m_ipV6 = std::move(ipAddresses.begin()->m_ipV6);
    return true;
}

void DnsResolver::cancel( RequestID reqID, bool waitForRunningHandlerCompletion )
{
    QnMutexLocker lk( &m_mutex );
    //TODO #ak improve search complexity
    m_taskQueue.remove_if( [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } );
    //we are waiting only for handler completion but not resolveAddressSync completion
    while( waitForRunningHandlerCompletion && (m_runningTaskReqID == reqID) )
        m_cond.wait( &m_mutex ); //waiting for completion
}

bool DnsResolver::isRequestIDKnown( RequestID reqID ) const
{
    QnMutexLocker lk( &m_mutex );
    //TODO #ak improve search complexity
    return m_runningTaskReqID == reqID ||
           std::find_if(
               m_taskQueue.cbegin(), m_taskQueue.cend(),
               [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } ) != m_taskQueue.cend();
}

void DnsResolver::run()
{
    QnMutexLocker lk( &m_mutex );
    while( !m_terminated )
    {
        while( m_taskQueue.empty() && !m_terminated )
            m_cond.wait( &m_mutex );
        if( m_terminated )
            break;  //not completing posted tasks

        ResolveTask task = m_taskQueue.front();

        lk.unlock();

        HostAddress resolvedAddress;
        const SystemError::ErrorCode resolveErrorCode =
            resolveAddressSync( task.hostAddress.toString(), &resolvedAddress, task.ipVersion )
            ? SystemError::noError
            : SystemError::getLastOSErrorCode();

        if( task.reqID )
        {
            lk.relock();
            if( m_taskQueue.empty() || (m_taskQueue.front().sequence != task.sequence) )
                continue;   //current task has been cancelled
            m_runningTaskReqID = task.reqID;
            m_taskQueue.pop_front();
            lk.unlock();
        }

        task.completionHandler( resolveErrorCode, resolvedAddress );

        lk.relock();
        m_runningTaskReqID = nullptr;
        m_cond.wakeAll();
    }
}

} // namespace network
} // namespace nx
