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

DnsResolver::ResolveTask::ResolveTask(
    HostAddress _hostAddress,
    std::function<void(SystemError::ErrorCode, const HostAddress&)> _completionHandler,
    RequestID _reqID,
    size_t _sequence )
:
    hostAddress( std::move( _hostAddress ) ),
    completionHandler( _completionHandler ),
    reqID( _reqID ),
    sequence( _sequence )
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
    RequestID reqID )
{
    QnMutexLocker lk( &m_mutex );
    m_taskQueue.push_back( ResolveTask( addressToResolve, completionHandler, reqID, ++m_currentSequence ) );
    m_cond.wakeAll();
}

bool DnsResolver::resolveAddressSync( const QString& hostName, HostAddress* const resolvedAddress )
{
    if( hostName.isEmpty() )
    {
        SystemError::setLastErrorCode( SystemError::hostNotFound );
        return false;
    }

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow only IPv4 */
    hints.ai_socktype = 0; /* Any socket */
    hints.ai_flags = AI_ALL;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    addrinfo* resolvedAddressInfo = nullptr;
    int status = getaddrinfo(hostName.toLatin1(), 0, &hints, &resolvedAddressInfo);

    if (status == EAI_BADFLAGS)
    {
        // if the lookup failed with AI_ALL, try again without it
        hints.ai_flags = 0;
        status = getaddrinfo(hostName.toLatin1(), 0, &hints, &resolvedAddressInfo);
    }

    if (status != 0)
    {
        // TODO: #mux Translate status into corresponding SystemError?
        SystemError::setLastErrorCode( SystemError::dnsServerFailure );
        return false;
    }

    resolvedAddress->m_sinAddr = ((struct sockaddr_in*)(resolvedAddressInfo->ai_addr))->sin_addr;
    resolvedAddress->m_addrStr = hostName;
    resolvedAddress->m_addressResolved = true;

    freeaddrinfo(resolvedAddressInfo);
    return true;
}

bool DnsResolver::isAddressResolved( const HostAddress& addr ) const
{
    return addr.m_addressResolved;
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
            resolveAddressSync( task.hostAddress.toString(), &resolvedAddress )
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

} // namespace nx
