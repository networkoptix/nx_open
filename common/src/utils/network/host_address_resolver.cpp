/**********************************************************
* 20 jan 2015
* a.kolesnikov
***********************************************************/

#include "host_address_resolver.h"

#include <algorithm>

#include <QMutexLocker>


HostAddressResolver::ResolveTask::ResolveTask(
    HostAddress _hostAddress,
    std::function<void(SystemError::ErrorCode, const HostAddress&)> _completionHandler,
    RequestID _reqID )
:
    hostAddress( _hostAddress ),
    completionHandler( _completionHandler ),
    reqID( _reqID )
{
}

HostAddressResolver::HostAddressResolver()
:
    m_terminated( false ),
    m_runningTaskReqID( nullptr )
{
    start();
}

HostAddressResolver::~HostAddressResolver()
{
    stop();
}

void HostAddressResolver::pleaseStop()
{
    QMutexLocker lk( &m_mutex );
    m_terminated = false;
    m_cond.wakeAll();
}

bool HostAddressResolver::resolveAddressAsync(
    const HostAddress& addressToResolve,
    std::function<void (SystemError::ErrorCode, const HostAddress&)>&& completionHandler,
    RequestID reqID )
{
    QMutexLocker lk( &m_mutex );
    m_taskQueue.push_back( ResolveTask( addressToResolve, completionHandler, reqID ) );
    m_cond.wakeAll();
    return true;
}

bool HostAddressResolver::resolveAddressSync( const QString& hostName, HostAddress* const resolvedAddress )
{
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
    if (status != 0)
        return false;

    resolvedAddress->m_sinAddr = ((struct sockaddr_in*)(resolvedAddressInfo->ai_addr))->sin_addr;
    resolvedAddress->m_addrStr = hostName;
    resolvedAddress->m_addressResolved = true;

    freeaddrinfo(resolvedAddressInfo);
    return true;
}

bool HostAddressResolver::isAddressResolved( const HostAddress& addr ) const
{
    return addr.m_addressResolved;
}

void HostAddressResolver::cancel( RequestID reqID, bool waitForRunningHandlerCompletion )
{
    QMutexLocker lk( &m_mutex );
    m_taskQueue.remove_if( [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } );
    while( waitForRunningHandlerCompletion && (m_runningTaskReqID == reqID) )
        m_cond.wait( &m_mutex ); //waiting for completion
}

bool HostAddressResolver::isRequestIDKnown( RequestID reqID ) const
{
    QMutexLocker lk( &m_mutex );
    return m_runningTaskReqID == reqID ||
           std::find_if(
               m_taskQueue.cbegin(), m_taskQueue.cend(),
               [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } ) != m_taskQueue.cend();
}

void HostAddressResolver::run()
{
    QMutexLocker lk( &m_mutex );
    while( !m_terminated )
    {
        while( m_taskQueue.empty() && !m_terminated )
            m_cond.wait( &m_mutex );
        if( m_terminated )
            break;  //not completing posted tasks

        ResolveTask task = std::move(m_taskQueue.front());
        m_taskQueue.pop_front();

        if( task.reqID )
            m_runningTaskReqID = task.reqID;

        lk.unlock();

        HostAddress resolvedAddress;
        resolveAddressSync( task.hostAddress.toString(), &resolvedAddress );
        task.completionHandler( SystemError::getLastOSErrorCode(), resolvedAddress );

        lk.relock();
        m_runningTaskReqID = nullptr;
        m_cond.wakeAll();
    }
}
