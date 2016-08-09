/**********************************************************
* 20 jan 2015
* a.kolesnikov
***********************************************************/

#include "host_address_resolver.h"

#ifdef Q_OS_UNIX
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <algorithm>
#include <atomic>


HostAddressResolver::ResolveTask::ResolveTask(
    HostAddress _hostAddress,
    std::function<void(SystemError::ErrorCode, const HostAddress&)> _completionHandler,
    RequestID _reqID,
    size_t _sequence )
:
    hostAddress( _hostAddress ),
    completionHandler( _completionHandler ),
    reqID( _reqID ),
    sequence( _sequence )
{
}

static std::atomic<HostAddressResolver*> HostAddressResolver_instance( nullptr );

HostAddressResolver::HostAddressResolver()
:
    m_terminated( false ),
    m_runningTaskReqID( nullptr ),
    m_currentSequence( 0 )
{
    start();

    Q_ASSERT( HostAddressResolver_instance.load() == nullptr );
    HostAddressResolver_instance = this;
}

HostAddressResolver::~HostAddressResolver()
{
    Q_ASSERT( HostAddressResolver_instance == this );
    HostAddressResolver_instance = nullptr;

    stop();
}

HostAddressResolver* HostAddressResolver::instance()
{
    SocketGlobalRuntime::instance();    //instanciates socket-related globals in correct order
    return HostAddressResolver_instance.load( std::memory_order_relaxed );
}

void HostAddressResolver::pleaseStop()
{
    QnMutexLocker lk( &m_mutex );
    m_terminated = true;
    m_cond.wakeAll();
}

bool HostAddressResolver::resolveAddressAsync(
    const HostAddress& addressToResolve,
    std::function<void (SystemError::ErrorCode, const HostAddress&)>&& completionHandler,
    RequestID reqID )
{
    QnMutexLocker lk( &m_mutex );
    m_taskQueue.push_back( ResolveTask( addressToResolve, completionHandler, reqID, ++m_currentSequence ) );
    m_cond.wakeAll();
    return true;
}

bool HostAddressResolver::resolveAddressSync( const QString& hostName, HostAddress* const resolvedAddress )
{
    if( hostName.isEmpty() )
    {
        SystemError::setLastErrorCode( SystemError::hostNotFound );
        return false;
    }

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_ALL;    /* For wildcard IP address */

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
        SystemError::setLastErrorCode( status );
        return false;
    }

    // TODO: support multi-address
    for (addrinfo* info = addressInfo; info; info = addressInfo->ai_next)
    {
        if (addressInfo->ai_family = AF_INET)
        {
            resolvedAddress->m_ipV4 = ((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr;
            freeaddrinfo(addressInfo);
            return true;
        }

        if (addressInfo->ai_family = AF_INET6)
        {
            resolvedAddress->m_ipV6 = ((struct sockaddr_in6*)(addressInfo->ai_addr))->sin6_addr;
            freeaddrinfo(addressInfo);
            return true;
        }
    }

    SystemError::setLastErrorCode( SystemError::hostNotFound );
    return true;
}

bool HostAddressResolver::resolveAddressSync(const HostAddress& host)
{
    if (isAddressResolved(host))
        return true;

    return resolveAddressSync(host.toString(), const_cast<HostAddress*>(&host));
}

bool HostAddressResolver::isAddressResolved( const HostAddress& addr ) const
{
    return addr.ipV4() || addr.ipV6();
}

void HostAddressResolver::cancel( RequestID reqID, bool waitForRunningHandlerCompletion )
{
    QnMutexLocker lk( &m_mutex );
    //TODO #ak improve search complexity
    m_taskQueue.remove_if( [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } );
    //we are waiting only for handler completion but not resolveAddressSync completion
    while( waitForRunningHandlerCompletion && (m_runningTaskReqID == reqID) )
        m_cond.wait( &m_mutex ); //waiting for completion
}

bool HostAddressResolver::isRequestIDKnown( RequestID reqID ) const
{
    QnMutexLocker lk( &m_mutex );
    //TODO #ak improve search complexity
    return m_runningTaskReqID == reqID ||
           std::find_if(
               m_taskQueue.cbegin(), m_taskQueue.cend(),
               [reqID]( const ResolveTask& task ){ return task.reqID == reqID; } ) != m_taskQueue.cend();
}

void HostAddressResolver::run()
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
