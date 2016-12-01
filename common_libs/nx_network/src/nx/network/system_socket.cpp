#include "system_socket.h"

#include <atomic>
#include <memory>
#include <boost/type_traits/is_same.hpp>

#include <utils/common/systemerror.h>
#include <utils/common/warnings.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/win32_syscall_resolver.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <common/common_globals.h>

#ifdef Q_OS_WIN
#  include <iphlpapi.h>
#  include <Mstcpip.h>
#  include "win32_socket_tools.h"
#else
#  include <netinet/tcp.h>
#endif

#include <QtCore/QElapsedTimer>

#include "aio/async_socket_helper.h"
#include "compat_poll.h"


#ifdef Q_OS_WIN
/* Check that the typedef in AbstractSocket is correct. */
static_assert(boost::is_same<AbstractSocket::SOCKET_HANDLE, SOCKET>::value, "Invalid socket type is used in AbstractSocket.");
typedef char raw_type;       // Type used for raw data on this platform
#else
#include <sys/types.h>       // For data types
#include <sys/socket.h>      // For socket(), connect(), send(), and recv()
#include <netdb.h>           // For getaddrinfo()
#include <arpa/inet.h>       // For inet_addr()
#include <unistd.h>          // For close()
#include <netinet/in.h>      // For sockaddr_in
#include <netinet/tcp.h>      // For TCP_NODELAY
#include <fcntl.h>
#include "ssl_socket.h"
typedef void raw_type;       // Type used for raw data on this platform
#endif

#ifdef WIN32
static bool initialized = false;
static const int ERR_TIMEOUT = WSAETIMEDOUT;
static const int ERR_WOULDBLOCK = WSAEWOULDBLOCK;
#else
static const int ERR_TIMEOUT = ETIMEDOUT;
//static const int ERR_WOULDBLOCK = EWOULDBLOCK;
#endif

int getSystemErrCode()
{
#ifdef WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}


#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

namespace nx {
namespace network {

SystemSocketAddress::SystemSocketAddress():
    size(0)
{
}

SystemSocketAddress::SystemSocketAddress(SocketAddress endpoint, int ipVersion):
    SystemSocketAddress()
{
    if (SocketGlobals::config().isHostDisabled(endpoint.address))
    {
        SystemError::setLastErrorCode(SystemError::noPermission);
        return;
    }

    if (ipVersion == AF_INET)
    {
        if (const auto ip = endpoint.address.ipV4())
        {
            const auto a = new sockaddr_in;
            memset(a, 0, sizeof(*a));
            ptr.reset((sockaddr*) a);
            size = sizeof(sockaddr_in);

            a->sin_family = AF_INET;
            a->sin_addr = *ip;
            a->sin_port = htons(endpoint.port);
            return;
        }
    }
    else if (ipVersion == AF_INET6)
    {
        if (const auto ip = endpoint.address.ipV6())
        {
            const auto a = new sockaddr_in6;
            memset(a, 0, sizeof(*a));
            ptr.reset((sockaddr*) a);
            size = sizeof(sockaddr_in6);

            a->sin6_family = AF_INET6;
            a->sin6_addr = *ip;
            a->sin6_port = htons(endpoint.port);
            return;
        }
    }

    SystemError::setLastErrorCode(SystemError::hostNotFound);
}

SystemSocketAddress::operator SocketAddress() const
{
    if (!ptr)
        return SocketAddress();

    if (ptr->sa_family == AF_INET)
    {
        const auto a = reinterpret_cast<const sockaddr_in*>(ptr.get());
        return SocketAddress(a->sin_addr, ntohs(a->sin_port));
    }
    else if (ptr->sa_family == AF_INET6)
    {
        const auto a = reinterpret_cast<const sockaddr_in6*>(ptr.get());
        return SocketAddress(a->sin6_addr, ntohs(a->sin6_port));
    }

    NX_ASSERT(false, lm("Corupt family: %1").arg(ptr->sa_family));
    return SocketAddress();
}

//////////////////////////////////////////////////////////
// Socket implementation
//////////////////////////////////////////////////////////
template<typename InterfaceToImplement>
Socket<InterfaceToImplement>::~Socket()
{
    close();
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getLastError(SystemError::ErrorCode* errorCode) const
{
    socklen_t optLen = sizeof(*errorCode);
    return getsockopt(m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(errorCode), &optLen) == 0;
}

template<typename InterfaceToImplement>
AbstractSocket::SOCKET_HANDLE Socket<InterfaceToImplement>::handle() const
{
    return Pollable::handle();
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getRecvTimeout(unsigned int* millis) const
{
    return Pollable::getRecvTimeout(millis);
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getSendTimeout(unsigned int* millis) const
{
    return Pollable::getSendTimeout(millis);
}

template<typename InterfaceToImplement>
aio::AbstractAioThread* Socket<InterfaceToImplement>::getAioThread() const
{
    return Pollable::getAioThread();
}

template<typename InterfaceToImplement>
void Socket<InterfaceToImplement>::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    return Pollable::bindToAioThread(aioThread);
}


template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::bind( const SocketAddress& localAddress )
{
    const SystemSocketAddress addr(localAddress, m_ipVersion);
    if (!addr.ptr)
        return false;

    return ::bind(m_fd, addr.ptr.get(), addr.size) == 0;
}

template<typename InterfaceToImplement>
SocketAddress Socket<InterfaceToImplement>::getLocalAddress() const
{
    if (m_ipVersion == AF_INET)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin_addr, ntohs(addr.sin_port));
    }
    else if (m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin6_addr, ntohs(addr.sin6_port));
    }

    return SocketAddress();
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::shutdown()
{
    if( m_fd == -1 )
        return true;

#ifdef Q_OS_WIN
    return ::shutdown(m_fd, SD_BOTH) == 0;
#else
    return ::shutdown(m_fd, SHUT_RDWR) == 0;
#endif
}


template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::close()
{
    shutdown();

    if( m_fd == -1 )
        return true;

    //checking that socket is not registered in aio
    NX_ASSERT(!nx::network::SocketGlobals::aioService().isSocketBeingWatched(static_cast<Pollable*>(this)));

    auto fd = m_fd;
    m_fd = -1;

#ifdef WIN32
    return ::closesocket(fd) == 0;
#else
    return ::close(fd) == 0;
#endif
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::isClosed() const
{
    return m_fd == -1;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setReuseAddrFlag( bool reuseAddr )
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
        qnWarning("Can't set SO_REUSEADDR flag to socket: %1.", SystemError::getLastOSErrorText());
        return false;
    }
    return true;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getReuseAddrFlag( bool* val ) const
{
    int reuseAddrVal = 0;
    socklen_t optLen = 0;

    if (::getsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
        return false;

    *val = reuseAddrVal > 0;
    return true;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setNonBlockingMode( bool val )
{
#ifdef _WIN32
    u_long _val = val ? 1 : 0;
    if( ioctlsocket( m_fd, FIONBIO, &_val ) == 0 )
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#else
    long currentFlags = fcntl( m_fd, F_GETFL, 0 );
    if( currentFlags == -1 )
        return false;
    if( val )
        currentFlags |= O_NONBLOCK;
    else
        currentFlags &= ~O_NONBLOCK;
    if( fcntl( m_fd, F_SETFL, currentFlags ) == 0 )
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#endif
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getNonBlockingMode( bool* val ) const
{
    *val = m_nonBlockingMode;
    return true;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getMtu( unsigned int* mtuValue ) const
{
#ifdef IP_MTU
    socklen_t optLen = 0;
    return ::getsockopt(m_fd, IPPROTO_IP, IP_MTU, (char*)mtuValue, &optLen) == 0;
#else
    *mtuValue = 1500;   //in winsock there is no IP_MTU, returning 1500 as most common value
    return true;
#endif
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setSendBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getSendBufferSize( unsigned int* buffSize ) const
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)buffSize, &optLen) == 0;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setRecvBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::getRecvBufferSize( unsigned int* buffSize ) const
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)buffSize, &optLen) == 0;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setRecvTimeout( unsigned int ms )
{
    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (m_fd, SOL_SOCKET, SO_RCVTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO,(const void *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<m_fd<<"). setRecvTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
        return false;
    }
    m_readTimeoutMS = ms;
    return true;
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::setSendTimeout( unsigned int ms )
{
    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (m_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&ms, sizeof(ms) ) != 0)
#else
    if (::setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO,(const char *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<m_fd<<"). setSendTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
        return false;
    }
    m_writeTimeoutMS = ms;
    return true;
}

template<typename InterfaceToImplement>
Pollable* Socket<InterfaceToImplement>::pollable()
{
    return this;
}

template<typename InterfaceToImplement>
void Socket<InterfaceToImplement>::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    nx::network::SocketGlobals::aioService().post(this, std::move(handler));
}

template<typename InterfaceToImplement>
void Socket<InterfaceToImplement>::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    nx::network::SocketGlobals::aioService().dispatch(this, std::move(handler));
}

template<typename InterfaceToImplement>
void Socket<InterfaceToImplement>::cleanUp()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

template<typename InterfaceToImplement>
unsigned short Socket<InterfaceToImplement>::resolveService(
    const QString &service,
    const QString &protocol)
{
    struct servent *serv;        /* Structure containing service information */

    if ((serv = getservbyname(service.toLatin1(), protocol.toLatin1())) == NULL)
        return atoi(service.toLatin1());  /* Service is port number */
    else
        return ntohs(serv->s_port);    /* Found port (network byte order) by name */
}

template<typename InterfaceToImplement>
Socket<InterfaceToImplement>::Socket(
    int type,
    int protocol,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
    createSocket( type, protocol );
}

template<typename InterfaceToImplement>
Socket<InterfaceToImplement>::Socket(
    int _sockDesc,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        _sockDesc,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
}

template<typename InterfaceToImplement>
bool Socket<InterfaceToImplement>::createSocket(int type, int protocol)
{
#ifdef WIN32
    if (!initialized) {
        WORD wVersionRequested;
        WSADATA wsaData;

        wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
        if (WSAStartup(wVersionRequested, &wsaData) != 0)  // Load WinSock DLL
            return false;
        initialized = true;
    }
#endif

    m_fd = ::socket(m_ipVersion, type, protocol);
    if( m_fd < 0 )
    {
        qWarning() << strerror(errno);
        return false;
    }

    if( m_ipVersion == AF_INET6 )
    {
        int off = 0;
        if( ::setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off)) )
            return false;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif
    return true;
}

//////////////////////////////////////////////////////////
///////// class CommunicatingSocket
//////////////////////////////////////////////////////////


// CommunicatingSocket Code

#ifndef _WIN32
namespace
{
    //static const size_t MILLIS_IN_SEC = 1000;
    //static const size_t NSECS_IN_MS = 1000000;

    template<class Func>
    int doInterruptableSystemCallWithTimeout( const Func& func, unsigned int timeout )
    {
        QElapsedTimer et;
        et.start();

        //struct timespec waitStartTime;
        //memset( &waitStartTime, 0, sizeof(waitStartTime) );
        bool waitStartTimeActual = false;
        if( timeout > 0 )
            waitStartTimeActual = true;  //clock_gettime( CLOCK_MONOTONIC, &waitStartTime ) == 0;
        for( ;; )
        {
            int result = func();
            //const int errCode = errno;
            if( result == -1 && errno == EINTR )
            {
                if( timeout == 0 ||  //no timeout
                    !waitStartTimeActual )  //cannot check timeout expiration
                {
                    continue;
                }
                //struct timespec waitStopTime;
                //memset( &waitStopTime, 0, sizeof(waitStopTime) );
                //if( clock_gettime( CLOCK_MONOTONIC, &waitStopTime ) != 0 )
                //    continue;   //not updating timeout value
                //const int millisAlreadySlept =
                //    ((uint64_t)waitStopTime.tv_sec*MILLIS_IN_SEC + waitStopTime.tv_nsec/NSECS_IN_MS) -
                //    ((uint64_t)waitStartTime.tv_sec*MILLIS_IN_SEC + waitStartTime.tv_nsec/NSECS_IN_MS);
                //if( (unsigned int)millisAlreadySlept < timeout )
                if( et.elapsed() < timeout )
                    continue;
                errno = ERR_TIMEOUT;    //operation timedout
            }
            return result;
        }
    }
}
#endif

template<typename InterfaceToImplement>
CommunicatingSocket<InterfaceToImplement>::CommunicatingSocket(
    int type,
    int protocol,
    int ipVersion,
    PollableSystemSocketImpl* sockImpl )
:
    Socket<InterfaceToImplement>(
        type,
        protocol,
        ipVersion,
        sockImpl),
    m_aioHelper(new aio::AsyncSocketImplHelper<SelfType>(this, ipVersion)),
    m_connected(false)
{
}

template<typename InterfaceToImplement>
CommunicatingSocket<InterfaceToImplement>::CommunicatingSocket(
    int newConnSD,
    int ipVersion,
    PollableSystemSocketImpl* sockImpl )
:
    Socket<InterfaceToImplement>(
        newConnSD,
        ipVersion,
        sockImpl),
    m_aioHelper(new aio::AsyncSocketImplHelper<SelfType>(this, ipVersion)),
    m_connected(true)   // This constructor is used by server socket.
{
}

template<typename InterfaceToImplement>
CommunicatingSocket<InterfaceToImplement>::~CommunicatingSocket()
{
    if (m_aioHelper)
        m_aioHelper->terminate();
}

template<typename InterfaceToImplement>
bool CommunicatingSocket<InterfaceToImplement>::connect(
    const SocketAddress& remoteAddress, unsigned int timeoutMs)
{
    if (remoteAddress.address.isIpAddress())
        return connectToIp(remoteAddress, timeoutMs);

    auto ips = SocketGlobals::addressResolver().dnsResolver().resolveSync(
        remoteAddress.address.toString(), this->m_ipVersion);

    while (!ips.empty())
    {
        auto ip = std::move(ips.front());
        ips.pop_front();
        if (connectToIp(SocketAddress(std::move(ip), remoteAddress.port), timeoutMs))
            return true;
    }

    return false; //< Could not connect by any of addresses.
}

template<typename InterfaceToImplement>
int CommunicatingSocket<InterfaceToImplement>::recv( void* buffer, unsigned int bufferLen, int flags )
{
#ifdef _WIN32
    int bytesRead;
    if (flags & MSG_DONTWAIT)
    {
        bool value;
        if (!getNonBlockingMode(&value))
            return -1;

        if (!setNonBlockingMode(true))
            return -1;

        bytesRead = ::recv(m_fd, (raw_type *) buffer, bufferLen, flags & ~MSG_DONTWAIT);

        // Save error code as changing mode will drop it.
        const auto sysErrorCodeBak = SystemError::getLastOSErrorCode();
        if (!setNonBlockingMode(value))
            return -1;

        SystemError::setLastErrorCode(sysErrorCodeBak);
    }
    else
    {
        bytesRead = ::recv(m_fd, (raw_type *) buffer, bufferLen, flags);
    }
#else
    unsigned int recvTimeout = 0;
    if( !this->getRecvTimeout( &recvTimeout ) )
        return -1;

    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::recv, this->m_fd, (void*)buffer, (size_t)bufferLen, flags),
        recvTimeout );
#endif
    if (bytesRead < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (errCode != SystemError::timedOut && errCode != SystemError::wouldBlock && errCode != SystemError::again)
            m_connected = false;
    }
    else if (bytesRead == 0)
        m_connected = false; //connection closed by remote host
    return bytesRead;
}

template<typename InterfaceToImplement>
int CommunicatingSocket<InterfaceToImplement>::send( const void* buffer, unsigned int bufferLen )
{
#ifdef _WIN32
    int sended = ::send(m_fd, (raw_type*) buffer, bufferLen, 0);
#else
    unsigned int sendTimeout = 0;
    if( !this->getSendTimeout( &sendTimeout ) )
        return -1;

    int sended = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::send, this->m_fd, (const void*)buffer, (size_t)bufferLen,
#ifdef __linux
            MSG_NOSIGNAL
#else
            0
#endif
    ),
        sendTimeout );
#endif
    if (sended < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (errCode != SystemError::timedOut && errCode != SystemError::wouldBlock && errCode != SystemError::again)
            m_connected = false;
    }
    else if (sended == 0)
        m_connected = false;
    return sended;
}

template<typename InterfaceToImplement>
SocketAddress CommunicatingSocket<InterfaceToImplement>::getForeignAddress() const
{
    if (this->m_ipVersion == AF_INET)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(this->m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin_addr, ntohs(addr.sin_port));
    }
    else if (this->m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(this->m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin6_addr, ntohs(addr.sin6_port));
    }

    return SocketAddress();
}

template<typename InterfaceToImplement>
bool CommunicatingSocket<InterfaceToImplement>::isConnected() const
{
    return m_connected;
}

template<typename InterfaceToImplement>
bool CommunicatingSocket<InterfaceToImplement>::close()
{
    m_connected = false;
    return Socket<InterfaceToImplement>::close();
}

template<typename InterfaceToImplement>
bool CommunicatingSocket<InterfaceToImplement>::shutdown()
{
    m_connected = false;
    return Socket<InterfaceToImplement>::shutdown();
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    m_aioHelper->cancelIOAsync(eventType, std::move(cancellationDoneHandler));
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::cancelIOSync(aio::EventType eventType)
{
    m_aioHelper->cancelIOSync(eventType);
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler )
{
    return m_aioHelper->connectAsync(addr, [handler = std::move(handler), this] (SystemError::ErrorCode code)
    {
        m_connected = (code == SystemError::noError);
        handler(code);
    });
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void( SystemError::ErrorCode, size_t )> handler )
{
    return m_aioHelper->readSomeAsync( buf, std::move( handler ) );
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::sendAsync(
    const nx::Buffer& buf,
    std::function<void( SystemError::ErrorCode, size_t )> handler )
{
    return m_aioHelper->sendAsync( buf, std::move( handler ) );
}

template<typename InterfaceToImplement>
void CommunicatingSocket<InterfaceToImplement>::registerTimer(
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> handler )
{
    //currently, aio considers 0 timeout as no timeout and will NOT call handler
    //NX_ASSERT(timeoutMs > std::chrono::milliseconds(0));
    if (timeoutMs == std::chrono::milliseconds(0))
        timeoutMs = std::chrono::milliseconds(1);  //handler of zero timer will NOT be called
    return m_aioHelper->registerTimer(timeoutMs, std::move(handler));
}

template<typename InterfaceToImplement>
bool CommunicatingSocket<InterfaceToImplement>::connectToIp(
    const SocketAddress& remoteAddress, unsigned int timeoutMs)
{
    // Get the address of the requested host
    m_connected = false;

    const SystemSocketAddress addr(remoteAddress, this->m_ipVersion);
    if (!addr.ptr)
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if( !this->getNonBlockingMode( &isNonBlockingModeBak ) )
        return false;
    if( !isNonBlockingModeBak && !this->setNonBlockingMode( true ) )
        return false;

    int connectResult = ::connect(this->m_fd, addr.ptr.get(), addr.size);

    if( connectResult != 0 )
    {
        if( SystemError::getLastOSErrorCode() != SystemError::inProgress )
            return false;
        if( isNonBlockingModeBak )
            return true;        //async connect started
    }

    int iSelRet = 0;

#ifdef _WIN32
    timeval timeVal;
    fd_set wrtFDS;

    /* monitor for incomming connections */
    FD_ZERO(&wrtFDS);
    FD_SET(m_fd, &wrtFDS);

    /* set timeout values */
    timeVal.tv_sec  = timeoutMs/1000;
    timeVal.tv_usec = timeoutMs%1000;
    iSelRet = ::select(
        m_fd + 1,
        NULL,
        &wrtFDS,
        NULL,
        timeoutMs >= 0 ? &timeVal : NULL );
#else
    //handling interruption by a signal
    //struct timespec waitStartTime;
    //memset( &waitStartTime, 0, sizeof(waitStartTime) );
    QElapsedTimer et;
    et.start();
    bool waitStartTimeActual = false;
    if( timeoutMs > 0 )
        waitStartTimeActual = true;  //clock_gettime( CLOCK_MONOTONIC, &waitStartTime ) == 0;
    for( ;; )
    {
        struct pollfd sockPollfd;
        memset( &sockPollfd, 0, sizeof(sockPollfd) );
        sockPollfd.fd = this->m_fd;
        sockPollfd.events = POLLOUT;
#ifdef _GNU_SOURCE
        sockPollfd.events |= POLLRDHUP;
#endif
        iSelRet = ::poll( &sockPollfd, 1, timeoutMs );


        //timeVal.tv_sec  = timeoutMs/1000;
        //timeVal.tv_usec = timeoutMs%1000;

        //iSelRet = ::select( m_fd + 1, NULL, &wrtFDS, NULL, timeoutMs >= 0 ? &timeVal : NULL );
        if( iSelRet == -1 && errno == EINTR )
        {
            //modifying timeout for time we've already spent in select
            if( timeoutMs == 0 ||  //no timeout
                !waitStartTimeActual )
            {
                //not updating timeout value. This can lead to spending "tcp connect timeout" in select (if signals arrive frequently and no monotonic clock on system)
                continue;
            }
            //struct timespec waitStopTime;
            //memset( &waitStopTime, 0, sizeof(waitStopTime) );
            //if( clock_gettime( CLOCK_MONOTONIC, &waitStopTime ) != 0 )
            //    continue;   //not updating timeout value
            const int millisAlreadySlept = et.elapsed();
            //    ((uint64_t)waitStopTime.tv_sec*MILLIS_IN_SEC + waitStopTime.tv_nsec/NSECS_IN_MS) -
            //    ((uint64_t)waitStartTime.tv_sec*MILLIS_IN_SEC + waitStartTime.tv_nsec/NSECS_IN_MS);
            if( millisAlreadySlept >= (int)timeoutMs )
                break;
            timeoutMs -= millisAlreadySlept;
            continue;
        }

        if ((sockPollfd.revents & POLLERR) || !(sockPollfd.revents & POLLOUT))
            iSelRet = 0;

        int result;
        socklen_t result_len = sizeof(result);
        if ((getsockopt(this->m_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) || (result != 0))
            iSelRet = 0;

        break;
    }
#endif

    m_connected = iSelRet > 0;

    //restoring original mode
    this->setNonBlockingMode( isNonBlockingModeBak );
    return m_connected;
}

//////////////////////////////////////////////////////////
///////// class TCPSocket
//////////////////////////////////////////////////////////

// TCPSocket Code

#ifdef _WIN32
class Win32TcpSocketImpl
:
    public PollableSystemSocketImpl
{
public:
    MIB_TCPROW win32TcpTableRow;

    Win32TcpSocketImpl()
    {
        memset( &win32TcpTableRow, 0, sizeof(win32TcpTableRow) );
    }
};
#endif

TCPSocket::TCPSocket(int ipVersion)
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
        )
{
}

TCPSocket::TCPSocket(int newConnSD, int ipVersion)
:
    base_type(
        newConnSD,
        ipVersion
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
        )
{
}

TCPSocket::~TCPSocket()
{
}

bool TCPSocket::reopen()
{
    close();
    return createSocket( SOCK_STREAM, IPPROTO_TCP );
}

bool TCPSocket::setNoDelay( bool value )
{
    int flag = value ? 1 : 0;
    return setsockopt( handle(),            // socket affected
                      IPPROTO_TCP,     // set option at TCP level
                      TCP_NODELAY,     // name of option
                      (char *) &flag,  // the cast is historical cruft
                      sizeof(int)) == 0;    // length of option value
}

//!Implementation of AbstractStreamSocket::getNoDelay
bool TCPSocket::getNoDelay( bool* value ) const
{
    int flag = 0;
    socklen_t optLen = 0;
    if( getsockopt( handle(),            // socket affected
                      IPPROTO_TCP,      // set option at TCP level
                      TCP_NODELAY,      // name of option
                      (char*)&flag,     // the cast is historical cruft
                      &optLen ) != 0 )  // length of option value
    {
        return false;
    }

    *value = flag > 0;
    return true;
}

bool TCPSocket::toggleStatisticsCollection( bool val )
{
#ifdef _WIN32
    //dynamically resolving functions that require win >= vista we want to use here
    typedef decltype(&SetPerTcpConnectionEStats) SetPerTcpConnectionEStatsType;
    static SetPerTcpConnectionEStatsType SetPerTcpConnectionEStatsAddr =
        Win32FuncResolver::instance()->resolveFunction<SetPerTcpConnectionEStatsType>
            ( L"Iphlpapi.dll", "SetPerTcpConnectionEStats" );

    if( SetPerTcpConnectionEStatsAddr == NULL )
        return false;


    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(impl());

    if( GetTcpRow(
            getLocalAddress().port,
            getForeignAddress().port,
            MIB_TCP_STATE_ESTAB,
            &d->win32TcpTableRow) != ERROR_SUCCESS )
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }

    auto freeLambda = [](void* ptr){ ::free(ptr); };
    std::unique_ptr<TCP_ESTATS_PATH_RW_v0, decltype(freeLambda)> pathRW( (TCP_ESTATS_PATH_RW_v0*)malloc( sizeof(TCP_ESTATS_PATH_RW_v0) ), freeLambda );
    if( !pathRW.get() )
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }

    memset( pathRW.get(), 0, sizeof(*pathRW) ); // zero the buffer
    pathRW->EnableCollection = val ? TRUE : FALSE;
    //enabling statistics collection
    if( SetPerTcpConnectionEStatsAddr(
            &d->win32TcpTableRow,
            TcpConnectionEstatsPath,
            (UCHAR*)pathRW.get(), 0, sizeof(*pathRW),
            0) != NO_ERROR )
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }
    return true;
#elif defined(__linux__)
    Q_UNUSED(val);
    return true;
#else
    Q_UNUSED(val);
    return false;
#endif
}

static const size_t USEC_PER_MSEC = 1000;

bool TCPSocket::getConnectionStatistics( StreamSocketInfo* info )
{
#ifdef _WIN32
    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(impl());

    if( !d->win32TcpTableRow.dwLocalAddr &&
        !d->win32TcpTableRow.dwLocalPort )
    {
        return false;
    }
    return readTcpStat( &d->win32TcpTableRow, info ) == ERROR_SUCCESS;
#elif defined(__linux__)
    struct tcp_info tcpinfo;
    memset( &tcpinfo, 0, sizeof(tcpinfo) );
    socklen_t tcp_info_length = sizeof(tcpinfo);
    if( getsockopt( handle(), SOL_TCP, TCP_INFO, (void *)&tcpinfo, &tcp_info_length ) != 0 )
        return false;
    info->rttVar = tcpinfo.tcpi_rttvar / USEC_PER_MSEC;
    return true;
#else
    Q_UNUSED(info);
    return false;
#endif
}

bool TCPSocket::setKeepAlive( boost::optional< KeepAliveOptions > info )
{
    #if defined( Q_OS_WIN )
        struct tcp_keepalive ka = { FALSE, 0, 0 };
        if( info )
        {
            ka.onoff = TRUE;
            ka.keepalivetime = info->timeSec * 1000; // s to ms
            ka.keepaliveinterval = info->intervalSec * 1000; // s to ms

            // the value can not be changed, 0 means default
            info->probeCount = 0;
        }

        DWORD length = sizeof( ka );
        if( WSAIoctl( handle(), SIO_KEEPALIVE_VALS,
                      &ka, sizeof(ka), NULL, 0, &length, NULL, NULL ) )
            return false;

        if( info )
            m_keepAlive = std::move( *info );
    #elif defined( Q_OS_LINUX )
        int isEnabled = info ? 1 : 0;
        if( setsockopt( handle(), SOL_SOCKET, SO_KEEPALIVE,
                        &isEnabled, sizeof(isEnabled) ) != 0 )
            return false;

        if( !info )
            return true;

        if( setsockopt( handle(), SOL_TCP, TCP_KEEPIDLE,
                        &info->timeSec, sizeof(info->timeSec) ) < 0 )
            return false;

        if( setsockopt( handle(), SOL_TCP, TCP_KEEPINTVL,
                        &info->intervalSec, sizeof(info->intervalSec) ) < 0 )
            return false;

        if( setsockopt( handle(), SOL_TCP, TCP_KEEPCNT,
                        &info->probeCount, sizeof(info->probeCount) ) < 0 )
            return false;
    #else
        int isEnabled = info ? 1 : 0;
        if( setsockopt( handle(), SOL_SOCKET, SO_KEEPALIVE,
                        &isEnabled, sizeof(isEnabled)) != 0 )
            return false;
    #endif

    return true;
}

bool TCPSocket::getKeepAlive( boost::optional< KeepAliveOptions >* result ) const
{
    int isEnabled = 0;
    socklen_t length = sizeof(isEnabled);
    if( getsockopt( handle(), SOL_SOCKET, SO_KEEPALIVE,
                    reinterpret_cast<char*>(&isEnabled), &length ) != 0 )
        return false;

    if (!isEnabled)
    {
        *result = boost::none;
        return true;
    }

    #if defined(Q_OS_WIN)
        *result = m_keepAlive;
    #elif defined(Q_OS_LINUX)
        KeepAliveOptions info;
        if( getsockopt( handle(), SOL_TCP, TCP_KEEPIDLE,
                        &info.timeSec, &length ) < 0 )
            return false;

        if( getsockopt( handle(), SOL_TCP, TCP_KEEPINTVL,
                        &info.intervalSec, &length ) < 0 )
            return false;

        if( getsockopt( handle(), SOL_TCP, TCP_KEEPCNT,
                        &info.probeCount, &length ) < 0 )
            return false;

        *result = std::move( info );
    #else
        *result = KeepAliveOptions();
    #endif

    return true;
}

//////////////////////////////////////////////////////////
///////// class TCPServerSocket
//////////////////////////////////////////////////////////

// TCPServerSocket Code

static const int DEFAULT_ACCEPT_TIMEOUT_MSEC = 250;
/*!
    \return fd (>=0) on success, <0 on error (-2 if timed out)
*/
static int acceptWithTimeout( int m_fd,
                              int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC,
                              bool nonBlockingMode = false )
{
    if (nonBlockingMode)
        return ::accept( m_fd, NULL, NULL );

    int result = 0;
    if (timeoutMillis == 0)
        timeoutMillis = -1; // poll expects -1 as an infinity

#ifdef _WIN32
    struct pollfd fds[1];
    memset( fds, 0, sizeof(fds) );
    fds[0].fd = m_fd;
    fds[0].events |= POLLIN;
    result = poll( fds, sizeof(fds)/sizeof(*fds), timeoutMillis );
    if( result < 0 )
        return result;
    if( result == 0 )   //timeout
    {
        ::SetLastError( SystemError::timedOut );
        return -1;
    }
    if( fds[0].revents & POLLERR )
    {
        int errorCode = 0;
        int errorCodeLen = sizeof( errorCode );
        if( getsockopt( m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &errorCodeLen ) != 0 )
            return -1;
        ::SetLastError( errorCode );
        return -1;
    }
    return ::accept(m_fd, NULL, NULL);
#else
    struct pollfd sockPollfd;
    memset( &sockPollfd, 0, sizeof(sockPollfd) );
    sockPollfd.fd = m_fd;
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    result = ::poll( &sockPollfd, 1, timeoutMillis );
    if( result < 0 )
        return result;
    if( result == 0 )   //timeout
    {
        errno = SystemError::timedOut;
        return -1;
    }
    if( sockPollfd.revents & POLLIN )
    {
        auto fd = ::accept( m_fd, NULL, NULL );
        return fd;
    }
    if( (sockPollfd.revents & POLLHUP)
#ifdef _GNU_SOURCE
        || (sockPollfd.revents & POLLRDHUP)
#endif
        )
    {
        errno = ENOTCONN;
        return -1;
    }
    if( sockPollfd.revents & POLLERR )
    {
        int errorCode = 0;
        socklen_t errorCodeLen = sizeof(errorCode);
        if( getsockopt( m_fd, SOL_SOCKET, SO_ERROR, &errorCode, &errorCodeLen ) != 0 )
            return -1;
        errno = errorCode;
        return -1;
    }
    return -1;
#endif
}

class TCPServerSocketPrivate
:
    public PollableSystemSocketImpl
{
public:
    int socketHandle;
    const int ipVersion;
    aio::AsyncServerSocketHelper<TCPServerSocket> asyncServerSocketHelper;

    TCPServerSocketPrivate( TCPServerSocket* _sock, int _ipVersion )
    :
        socketHandle( -1 ),
        ipVersion( _ipVersion ),
        asyncServerSocketHelper( _sock )
    {
    }

    AbstractStreamSocket* accept( unsigned int recvTimeoutMs, bool nonBlockingMode )
    {
        int newConnSD = acceptWithTimeout( socketHandle, recvTimeoutMs, nonBlockingMode );
        if( newConnSD >= 0 )
        {
            return new TCPSocket(newConnSD, ipVersion);
        }
        else if( newConnSD == -2 )
        {
            //setting system error code
    #ifdef _WIN32
            ::SetLastError( SystemError::timedOut );
    #else
            errno = SystemError::timedOut;
    #endif
            return nullptr;    //timeout
        }
        else
        {
            return nullptr;

        }
    }
};

TCPServerSocket::TCPServerSocket(int ipVersion)
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion,
        new TCPServerSocketPrivate( this, ipVersion ) )
{
    static_cast<TCPServerSocketPrivate*>(impl())->socketHandle = handle();
}

TCPServerSocket::~TCPServerSocket()
{
    if (isInSelfAioThread())
    {
        TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
        d->asyncServerSocketHelper.stopPolling();
        return;
    }

    NX_CRITICAL(
        !nx::network::SocketGlobals::aioService()
            .isSocketBeingWatched(static_cast<Pollable*>(this)),
        "You MUST cancel running async socket operation before "
        "deleting socket if you delete socket from non-aio thread");
}

int TCPServerSocket::accept(int sockDesc)
{
    return acceptWithTimeout( sockDesc );
}

void TCPServerSocket::acceptAsync(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> handler)
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.acceptAsync( std::move(handler) );
}

void TCPServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.cancelIOAsync(std::move(handler));
}

void TCPServerSocket::cancelIOSync()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.cancelIOSync();
}

//!Implementation of AbstractStreamServerSocket::listen
bool TCPServerSocket::listen(int queueLen)
{
    return ::listen( handle(), queueLen ) == 0;
}

void TCPServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    //TODO #ak add general implementation to Socket class and remove this method
    dispatch(
        [this, completionHandler = std::move(completionHandler)]()
        {
            TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
            d->asyncServerSocketHelper.stopPolling();

            completionHandler();
        });
}

void TCPServerSocket::pleaseStopSync(bool /*assertIfCalledUnderLock*/)
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    d->asyncServerSocketHelper.cancelIOSync();
}

//!Implementation of AbstractStreamServerSocket::accept
AbstractStreamSocket* TCPServerSocket::accept()
{
    return systemAccept();
}

AbstractStreamSocket* TCPServerSocket::systemAccept()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());

    unsigned int recvTimeoutMs = 0;
    if (!getRecvTimeout(&recvTimeoutMs))
        return nullptr;

    bool nonBlockingMode = false;
    if (!getNonBlockingMode(&nonBlockingMode))
        return nullptr;

    auto* acceptedSocket = d->accept(recvTimeoutMs, nonBlockingMode);
    if (!acceptedSocket)
        return nullptr;
#ifdef _WIN32
    if (!nonBlockingMode)
        return acceptedSocket;

    //moving socket to blocking mode by default to be consistent with msdn
        //(https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573(v=vs.85).aspx)
    if (!acceptedSocket->setNonBlockingMode(false))
    {
        delete acceptedSocket;
        return nullptr;
    }
#endif
    return acceptedSocket;
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen( handle(), queueLen ) == 0;
}


//////////////////////////////////////////////////////////
///////// class UDPSocket
//////////////////////////////////////////////////////////

// UDPSocket Code

UDPSocket::UDPSocket(int ipVersion)
:
    base_type(SOCK_DGRAM, IPPROTO_UDP, ipVersion),
    m_destAddr()
{
    setBroadcast();
    int buff_size = 1024*512;
    if( ::setsockopt( handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof( buff_size ) )<0 )
    {
        //error
    }
}

void UDPSocket::setBroadcast() {
    // If this fails, we'll hear about it when we try to send.  This will allow
    // system that cannot broadcast to continue if they don't plan to broadcast
    int broadcastPermission = 1;
    setsockopt( handle(), SOL_SOCKET, SO_BROADCAST,
               (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

bool UDPSocket::sendTo(const void *buffer, int bufferLen)
{
    // Write out the whole buffer as a single message.
    #ifdef _WIN32
        return sendto(
            handle(), (raw_type *)buffer, bufferLen, 0,
            m_destAddr.ptr.get(), m_destAddr.size) == bufferLen;
    #else
        unsigned int sendTimeout = 0;
        if (!getSendTimeout(&sendTimeout))
            return -1;

        #ifdef __linux__
            int flags = MSG_NOSIGNAL;
        #else
            int flags = 0;
        #endif

        return doInterruptableSystemCallWithTimeout<>(
            std::bind(
                &::sendto, handle(), (const void*)buffer, (size_t)bufferLen, flags,
                m_destAddr.ptr.get(), m_destAddr.size),
            sendTimeout) == bufferLen;
    #endif

}

bool UDPSocket::setMulticastTTL(unsigned char multicastTTL)  {
    if( setsockopt( handle(), IPPROTO_IP, IP_MULTICAST_TTL,
                   (raw_type *) &multicastTTL, sizeof(multicastTTL)) < 0) {
        qnWarning("Multicast TTL set failed (setsockopt()).");
        return false;
    }
    return true;
}

bool UDPSocket::setMulticastIF(const QString& multicastIF)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(multicastIF.toLatin1().data());
    if( setsockopt( handle(), IPPROTO_IP, IP_MULTICAST_IF, (raw_type *)&localInterface, sizeof( localInterface ) ) < 0 )
    {
        qnWarning("Multicast TTL set failed (setsockopt()).");
        return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const QString &multicastGroup)  {
    struct ip_mreq multicastRequest;

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if( setsockopt( handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
        (raw_type *) &multicastRequest,
        sizeof(multicastRequest)) < 0) {
            qWarning() << "failed to join multicast group" << multicastGroup;
            return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const QString &multicastGroup, const QString& multicastIF)  {
    struct ip_mreq multicastRequest;

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toLatin1());
    if( setsockopt( handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
        (raw_type *) &multicastRequest,
        sizeof(multicastRequest)) < 0) {
            qWarning() << "failed to join multicast group" << multicastGroup << "from IF" << multicastIF<<". "<<SystemError::getLastOSErrorText();
            return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const QString &multicastGroup)  {
    struct ip_mreq multicastRequest;

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if( setsockopt( handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type *) &multicastRequest,
        sizeof(multicastRequest)) < 0) {
            qnWarning("Multicast group leave failed (setsockopt()).");
            return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const QString &multicastGroup, const QString& multicastIF)  {
    struct ip_mreq multicastRequest;

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toLatin1());
    if( setsockopt( handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type *) &multicastRequest,
        sizeof(multicastRequest)) < 0) {
            qnWarning("Multicast group leave failed (setsockopt()).");
            return false;
    }
    return true;
}

int UDPSocket::send( const void* buffer, unsigned int bufferLen )
{
    #ifdef _WIN32
        return sendto(
            handle(), (raw_type *)buffer, bufferLen, 0,
            m_destAddr.ptr.get(), m_destAddr.size);
    #else
        unsigned int sendTimeout = 0;
        if (!getSendTimeout(&sendTimeout))
            return -1;

        return doInterruptableSystemCallWithTimeout<>(
            std::bind(
                &::sendto, handle(),
                (const void*)buffer, (size_t)bufferLen, 0,
                m_destAddr.ptr.get(), m_destAddr.size),
            sendTimeout);
    #endif

}

//!Implementation of AbstractDatagramSocket::setDestAddr
bool UDPSocket::setDestAddr( const SocketAddress& endpoint )
{
    if (endpoint.address.isIpAddress())
    {
        m_destAddr = SystemSocketAddress(endpoint, m_ipVersion);
    }
    else
    {
        const auto ips = SocketGlobals::addressResolver().dnsResolver().resolveSync(
            endpoint.address.toString(), m_ipVersion);

        // TODO: Here we select first address with hope it is correct one. This will never work
        // for NAT64, so we have to fix it somehow.
        if (!ips.empty())
            m_destAddr = SystemSocketAddress(SocketAddress(ips.front(), endpoint.port), m_ipVersion);
    }

    return (bool) m_destAddr.ptr;
}

//!Implementation of AbstractDatagramSocket::sendTo
bool UDPSocket::sendTo(
    const void* buffer,
    unsigned int bufferLen,
    const SocketAddress& foreignEndpoint )
{
    setDestAddr( foreignEndpoint );
    return sendTo( buffer, bufferLen );
}

void UDPSocket::sendToAsync(
    const nx::Buffer& buffer,
    const SocketAddress& endpoint,
    std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> handler)
{
    if (endpoint.address.isIpAddress())
    {
        setDestAddr(endpoint);
        return sendAsync(
            buffer,
            [endpoint, handler = std::move(handler)](SystemError::ErrorCode code, size_t size)
            {
                handler(code, endpoint, size);
            });
    }

    m_aioHelper->resolve(
        endpoint.address,
        [this, &buffer, port = endpoint.port, handler = std::move(handler)](
            SystemError::ErrorCode code, std::deque<HostAddress> ips) mutable
        {
            if (code != SystemError::noError)
                return handler(code, SocketAddress(), 0);

            // TODO: Here we select first address with hope it is correct one. This will never work
            // for NAT64, so we have to fix it somehow.
            sendToAsync(buffer, SocketAddress(ips.front(), port), std::move(handler));
        });
}

int UDPSocket::recv( void* buffer, unsigned int bufferLen, int /*flags*/ )
{
    //TODO #ak use flags
    return recvFrom( buffer, bufferLen, &m_prevDatagramAddress.address, &m_prevDatagramAddress.port );
}

int UDPSocket::recvFrom(
    void *buffer,
    unsigned int bufferLen,
    SocketAddress* const sourceAddress )
{
    int rtn = recvFrom(
        buffer,
        bufferLen,
        &m_prevDatagramAddress.address,
        &m_prevDatagramAddress.port );

    if( rtn >= 0 && sourceAddress )
        *sourceAddress = m_prevDatagramAddress;
    return rtn;
}

void UDPSocket::recvFromAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> handler)
{
    //TODO #ak #msvc2015 move handler
    readSomeAsync(
        buf,
        [/*std::move*/ handler, this](SystemError::ErrorCode errCode, size_t bytesRead){
            handler(errCode, std::move(m_prevDatagramAddress), bytesRead);
        });
}

SocketAddress UDPSocket::lastDatagramSourceAddress() const
{
    return m_prevDatagramAddress;
}

bool UDPSocket::hasData() const
{
#ifdef _WIN32
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET( handle(), &read_set );
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    switch( ::select(FD_SETSIZE, &read_set, NULL, NULL, &timeout))
    {
        case 0:             // timeout expired
            return false;
        case SOCKET_ERROR:  // error occured
            return false;
    }
    return true;
#else
    struct pollfd sockPollfd;
    memset( &sockPollfd, 0, sizeof(sockPollfd) );
    sockPollfd.fd = handle();
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    return (::poll( &sockPollfd, 1, 0 ) == 1) && ((sockPollfd.revents & POLLIN) != 0);
#endif
}

int UDPSocket::recvFrom(
    void* buffer,
    unsigned int bufferLen,
    HostAddress* const sourceAddress,
    quint16* const sourcePort )
{
    sockaddr_in clntAddr;
    socklen_t addrLen = sizeof( clntAddr );

#ifdef _WIN32
    int rtn = recvfrom( handle(), (raw_type *)buffer, bufferLen, 0, (sockaddr *)&clntAddr, (socklen_t *)&addrLen );
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int rtn = doInterruptableSystemCallWithTimeout<>(
        std::bind( &::recvfrom, handle(), (void*)buffer, (size_t)bufferLen, 0, (sockaddr*)&clntAddr, (socklen_t*)&addrLen ),
        recvTimeout );
#endif

    if( rtn >= 0 )
    {
        *sourceAddress = HostAddress( clntAddr.sin_addr );
        *sourcePort = ntohs(clntAddr.sin_port);
    }
    return rtn;
}

template class Socket<AbstractStreamServerSocket>;
template class Socket<AbstractStreamSocket>;
template class CommunicatingSocket<AbstractStreamSocket>;
template class CommunicatingSocket<AbstractDatagramSocket>;

}   //network
}   //nx
