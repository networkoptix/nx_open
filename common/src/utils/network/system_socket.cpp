#include "system_socket.h"

#include <atomic>
#include <memory>

#include <boost/type_traits/is_same.hpp>

#include <platform/win32_syscall_resolver.h>
#include <utils/common/systemerror.h>
#include <utils/common/warnings.h>
#include <utils/network/ssl_socket.h>
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>
#include <common/common_globals.h>

#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  include "win32_socket_tools.h"
#else
#  include <netinet/tcp.h>
#endif

#include <QtCore/QElapsedTimer>

#include "aio/async_socket_helper.h"
#include "compat_poll.h"
#include "utils/common/log.h"


#ifdef Q_OS_WIN
/* Check that the typedef in AbstractSocket is correct. */
static_assert(boost::is_same<AbstractSocket::SOCKET_HANDLE, SOCKET>::value, "Invalid socket type is used in AbstractSocket.");
typedef int socklen_t;
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

//////////////////////////////////////////////////////////
// Socket implementation
//////////////////////////////////////////////////////////

Socket::~Socket() {
    close();
}


//!Implementation of AbstractSocket::bind
bool Socket::bind( const SocketAddress& localAddress )
{
    const auto addr = makeAddr(localAddress);
    if (!addr.ptr)
        return false;

    return ::bind(m_fd, addr.ptr.get(), addr.size) == 0;
}

////!Implementation of AbstractSocket::bindToInterface
//bool Socket::bindToInterface( const QnInterfaceAndAddr& iface )
//{
//#ifdef Q_OS_LINUX
//    setLocalPort(0);
//    bool res = setsockopt(handle(), SOL_SOCKET, SO_BINDTODEVICE, iface.name.toLatin1().constData(), iface.name.length()) >= 0;
//#else
//    bool res = setLocalAddressAndPort(iface.address.toString(), 0);
//#endif
//
//    //if (!res)
//    //    qnDebug("Can't bind to interface %1. Error code %2.", iface.address.toString(), strerror(errno));
//    return res;
//}

//!Implementation of AbstractSocket::getLocalAddress
SocketAddress Socket::getLocalAddress() const
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

void Socket::shutdown()
{
    if( m_fd == -1 )
        return;

#ifdef Q_OS_WIN
    ::shutdown(m_fd, SD_BOTH);
#else
    ::shutdown(m_fd, SHUT_RDWR);
#endif
}


//!Implementation of AbstractSocket::close
void Socket::close()
{
    shutdown();

    if( m_fd == -1 )
        return;

#ifdef WIN32
    ::closesocket(m_fd);
#else
    ::close(m_fd);
#endif
    m_fd = -1;
}

bool Socket::isClosed() const
{
    return m_fd == -1;
}

//!Implementation of AbstractSocket::setReuseAddrFlag
bool Socket::setReuseAddrFlag( bool reuseAddr )
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
        qnWarning("Can't set SO_REUSEADDR flag to socket: %1.", SystemError::getLastOSErrorText());
        return false;
    }
    return true;
}

//!Implementation of AbstractSocket::reuseAddrFlag
bool Socket::getReuseAddrFlag( bool* val )
{
    int reuseAddrVal = 0;
    socklen_t optLen = 0;

    if (::getsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
        return false;

    *val = reuseAddrVal > 0;
    return true;
}

//!Implementation of AbstractSocket::setNonBlockingMode
bool Socket::setNonBlockingMode( bool val )
{
    if( val == m_nonBlockingMode )
        return true;

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

//!Implementation of AbstractSocket::getNonBlockingMode
bool Socket::getNonBlockingMode( bool* val ) const
{
    *val = m_nonBlockingMode;
    return true;
}

//!Implementation of AbstractSocket::getMtu
bool Socket::getMtu( unsigned int* mtuValue )
{
#ifdef IP_MTU
    socklen_t optLen = 0;
    return ::getsockopt(m_fd, IPPROTO_IP, IP_MTU, (char*)mtuValue, &optLen) == 0;
#else
    *mtuValue = 1500;   //in winsock there is no IP_MTU, returning 1500 as most common value
    return true;
#endif
}

//!Implementation of AbstractSocket::setSendBufferSize
bool Socket::setSendBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getSendBufferSize
bool Socket::getSendBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvBufferSize
bool Socket::setRecvBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getRecvBufferSize
bool Socket::getRecvBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvTimeout
bool Socket::setRecvTimeout( unsigned int ms )
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

//!Implementation of AbstractSocket::setSendTimeout
bool Socket::setSendTimeout( unsigned int ms )
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

bool Socket::getLastError( SystemError::ErrorCode* errorCode )
{
    socklen_t optLen = sizeof(*errorCode);
    return getsockopt(m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(errorCode), &optLen) == 0;
}

void Socket::postImpl( std::function<void()>&& handler )
{
    m_baseAsyncHelper->post( std::move(handler) );
}

void Socket::dispatchImpl( std::function<void()>&& handler )
{
    m_baseAsyncHelper->dispatch( std::move(handler) );
}

void Socket::cleanUp()  {
#ifdef _WIN32
    WSACleanup();
#endif
}

unsigned short Socket::resolveService(const QString &service,
                                      const QString &protocol) {
    struct servent *serv;        /* Structure containing service information */

    if ((serv = getservbyname(service.toLatin1(), protocol.toLatin1())) == NULL)
        return atoi(service.toLatin1());  /* Service is port number */
    else
        return ntohs(serv->s_port);    /* Found port (network byte order) by name */
}

Socket::Socket(
    std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>> asyncHelper,
    int type,
    int protocol,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_baseAsyncHelper( std::move(asyncHelper) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
    createSocket( type, protocol );
}

Socket::Socket(
    std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>> asyncHelper,
    int _sockDesc,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        _sockDesc,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_baseAsyncHelper( std::move(asyncHelper) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
}

Socket::Socket(
    int type,
    int protocol,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_baseAsyncHelper( new BaseAsyncSocketImplHelper<Pollable>(this) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
    createSocket( type, protocol );
}

Socket::Socket(
    int _sockDesc,
    int ipVersion,
    PollableSystemSocketImpl* impl )
:
    Pollable(
        _sockDesc,
        std::unique_ptr<PollableSystemSocketImpl>(impl) ),
    m_baseAsyncHelper( new BaseAsyncSocketImplHelper<Pollable>(this) ),
    m_ipVersion( ipVersion ),
    m_nonBlockingMode( false )
{
}

// Function to fill in address structure given an address and port
Socket::SockAddrPtr Socket::makeAddr(const SocketAddress& socketAddress)
{
    // NOTE: blocking dns name resolve may happen here
    if (!HostAddressResolver::isAddressResolved(socketAddress.address) &&
        !HostAddressResolver::instance()->resolveAddressSync(
            socketAddress.address.toString(),
            const_cast<HostAddress*>(&socketAddress.address)))
    {
        return SockAddrPtr();
    }

    if (m_ipVersion == AF_INET)
    {
        if (const auto ip = socketAddress.address.ipV4())
        {
            const auto addr = new sockaddr_in;
            SockAddrPtr addrHolder(addr);

            addr->sin_family = AF_INET;
            addr->sin_addr = *ip;
            addr->sin_port = htons(socketAddress.port);
            return addrHolder;
        }
    }
    else if (m_ipVersion == AF_INET6)
    {
        if (const auto ip = socketAddress.address.ipV6())
        {
            const auto addr = new sockaddr_in6;
            SockAddrPtr addrHolder(addr);

            addr->sin6_family = AF_INET6;
            addr->sin6_addr = *ip;
            addr->sin6_port = htons(socketAddress.port);
            return addrHolder;
        }
    }

    SystemError::setLastErrorCode(SystemError::hostNotFound);
    return SockAddrPtr();
}

bool Socket::createSocket(int type, int protocol)
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
        return false;

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

CommunicatingSocket::CommunicatingSocket(
    AbstractCommunicatingSocket* abstractSocketPtr,
    int type,
    int protocol,
    int ipVersion,
    PollableSystemSocketImpl* sockImpl )
:
    Socket(
        std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>>(
            new AsyncSocketImplHelper<Pollable>( this, abstractSocketPtr ) ),
        type,
        protocol,
        ipVersion,
        sockImpl ),
    m_aioHelper( nullptr ),
    m_connected( false )
{
    m_aioHelper = static_cast<AsyncSocketImplHelper<Pollable>*>(this->m_baseAsyncHelper.get());
}

CommunicatingSocket::CommunicatingSocket(
    AbstractCommunicatingSocket* abstractSocketPtr,
    int newConnSD,
    int ipVersion,
    PollableSystemSocketImpl* sockImpl )
:
    Socket(
        std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>>(
            new AsyncSocketImplHelper<Pollable>( this, abstractSocketPtr ) ),
        newConnSD,
        ipVersion,
        sockImpl ),
    m_aioHelper( nullptr ),
    m_connected( true )   //this constructor is used is server socket
{
    m_aioHelper = static_cast<AsyncSocketImplHelper<Pollable>*>(this->m_baseAsyncHelper.get());
}

CommunicatingSocket::~CommunicatingSocket()
{
    m_aioHelper->terminate();
}

void CommunicatingSocket::terminateAsyncIO( bool waitForRunningHandlerCompletion )
{
    m_aioHelper->terminateAsyncIO();    //all futher async operations will be ignored
    m_aioHelper->cancelAsyncIO( aio::etNone, waitForRunningHandlerCompletion );
}

//!Implementation of AbstractCommunicatingSocket::connect
bool CommunicatingSocket::connect( const SocketAddress& remoteAddress, unsigned int timeoutMs )
{
    // Get the address of the requested host
    m_connected = false;

    const auto addr = makeAddr(remoteAddress);
    if (!addr.ptr)
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if( !getNonBlockingMode( &isNonBlockingModeBak ) )
        return false;
    if( !isNonBlockingModeBak && !setNonBlockingMode( true ) )
        return false;

    int connectResult = ::connect(m_fd, addr.ptr.get(), addr.size);

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
        sockPollfd.fd = m_fd;
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

        if( (sockPollfd.revents & POLLOUT) == 0 )
            iSelRet = 0;
        break;
    }
#endif

    m_connected = iSelRet > 0;

    //restoring original mode
    setNonBlockingMode( isNonBlockingModeBak );
    return m_connected;
}

//!Implementation of AbstractCommunicatingSocket::recv
int CommunicatingSocket::recv( void* buffer, unsigned int bufferLen, int flags )
{
#ifdef _WIN32
    int bytesRead = ::recv(m_fd, (raw_type *) buffer, bufferLen, flags);
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::recv, m_fd, (void*)buffer, (size_t)bufferLen, flags),
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

//!Implementation of AbstractCommunicatingSocket::send
int CommunicatingSocket::send( const void* buffer, unsigned int bufferLen )
{
#ifdef _WIN32
    int sended = ::send(m_fd, (raw_type*) buffer, bufferLen, 0);
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    int sended = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::send, m_fd, (const void*)buffer, (size_t)bufferLen,
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

//!Implementation of AbstractCommunicatingSocket::getForeignAddress
SocketAddress CommunicatingSocket::getForeignAddress() const
{
    if (m_ipVersion == AF_INET)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin_addr, ntohs(addr.sin_port));
    }
    else if (m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin6_addr, ntohs(addr.sin6_port));
    }

    return SocketAddress();
}

//!Implementation of AbstractCommunicatingSocket::isConnected
bool CommunicatingSocket::isConnected() const
{
    return m_connected;
}

void CommunicatingSocket::close()
{
    //checking that socket is not registered in aio
    assert( !aio::AIOService::instance()->isSocketBeingWatched( static_cast<Pollable*>(this) ) );

    m_connected = false;
    Socket::close();
}

void CommunicatingSocket::shutdown()
{
    m_connected = false;
    Socket::shutdown();
}

void CommunicatingSocket::cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion )
{
    m_aioHelper->cancelAsyncIO( eventType, waitForRunningHandlerCompletion );
}

bool CommunicatingSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
{
    return m_aioHelper->connectAsyncImpl( addr, std::move(handler) );
}

bool CommunicatingSocket::recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
{
    return m_aioHelper->recvAsyncImpl( buf, std::move( handler ) );
}

bool CommunicatingSocket::sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
{
    return m_aioHelper->sendAsyncImpl( buf, std::move( handler ) );
}

bool CommunicatingSocket::registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler )
{
    return m_aioHelper->registerTimerImpl( timeoutMs, std::move( handler ) );
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
    return m_implDelegate.createSocket( SOCK_STREAM, IPPROTO_TCP );
}

bool TCPSocket::setNoDelay( bool value )
{
    int flag = value ? 1 : 0;
    return setsockopt( m_implDelegate.handle(),            // socket affected
                      IPPROTO_TCP,     // set option at TCP level
                      TCP_NODELAY,     // name of option
                      (char *) &flag,  // the cast is historical cruft
                      sizeof(int)) == 0;    // length of option value
}

//!Implementation of AbstractStreamSocket::getNoDelay
bool TCPSocket::getNoDelay( bool* value )
{
    int flag = 0;
    socklen_t optLen = 0;
    if( getsockopt( m_implDelegate.handle(),            // socket affected
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


    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(m_implDelegate.impl());

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
    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(m_implDelegate.impl());

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
    if( getsockopt( m_implDelegate.handle(), SOL_TCP, TCP_INFO, (void *)&tcpinfo, &tcp_info_length ) != 0 )
        return false;
    info->rttVar = tcpinfo.tcpi_rttvar / USEC_PER_MSEC;
    return true;
#else
    Q_UNUSED(info);
    return false;
#endif
}


//////////////////////////////////////////////////////////
///////// class TCPServerSocket
//////////////////////////////////////////////////////////

// TCPServerSocket Code

static const int DEFAULT_ACCEPT_TIMEOUT_MSEC = 250;
/*!
    \return fd (>=0) on success, <0 on error (-2 if timed out)
*/
static int acceptWithTimeout( int m_fd, int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC )
{
    int result = 0;

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
    return ::accept( m_fd, NULL, NULL );
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
    public PollableSystemSocketImpl,
    public aio::AIOEventHandler<Pollable>
{
public:
    std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )> acceptHandler;
    int socketHandle;
    const int ipVersion;
    std::atomic<int> acceptAsyncCallCount;

    TCPServerSocketPrivate( TCPServerSocket* _sock, int _ipVersion )
    :
        socketHandle( -1 ),
        ipVersion( _ipVersion ),
        acceptAsyncCallCount( 0 ),
        m_sock( _sock )
    {
    }

    virtual void eventTriggered( Pollable* sock, aio::EventType eventType ) throw() override
    {
        assert( acceptHandler );

        const int acceptAsyncCallCountBak = acceptAsyncCallCount;

        switch( eventType )
        {
            case aio::etRead:
            {
                //accepting socket
                AbstractStreamSocket* newSocket = m_sock->accept();
                acceptHandler(
                    newSocket != nullptr ? SystemError::noError : SystemError::getLastOSErrorCode(),
                    newSocket );
                break;
            }

            case aio::etReadTimedOut:
                acceptHandler( SystemError::timedOut, nullptr );
                break;

            case aio::etError:
            {
                SystemError::ErrorCode errorCode = SystemError::noError;
                m_sock->getLastError( &errorCode );
                acceptHandler( errorCode, nullptr );
                break;
            }

            default:
                assert( false );
                break;
        }

        //if asyncAccept has been called from onNewConnection, no need to call removeFromWatch
        if( acceptAsyncCallCount > acceptAsyncCallCountBak )
            return;

        aio::AIOService::instance()->removeFromWatch( sock, aio::etRead );
        acceptHandler = std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>();
    }

    AbstractStreamSocket* accept( unsigned int recvTimeoutMs )
    {
        int newConnSD = acceptWithTimeout( socketHandle, recvTimeoutMs );
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

private:
    TCPServerSocket* m_sock;
};

TCPServerSocket::TCPServerSocket(int ipVersion)
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion,
        new TCPServerSocketPrivate( this, ipVersion ) )
{
    static_cast<TCPServerSocketPrivate*>(m_implDelegate.impl())->socketHandle = m_implDelegate.handle();
    setRecvTimeout( DEFAULT_ACCEPT_TIMEOUT_MSEC );
}

int TCPServerSocket::accept(int sockDesc)
{
    return acceptWithTimeout( sockDesc );
}

bool TCPServerSocket::acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>&& handler )
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(m_implDelegate.impl());

    ++d->acceptAsyncCallCount;

    d->acceptHandler = std::move(handler);
    //TODO: #ak usually acceptAsyncImpl is called repeatedly. SHOULD avoid unneccessary watchSocket and removeFromWatch calls
    return aio::AIOService::instance()->watchSocket( static_cast<Pollable*>(&m_implDelegate), aio::etRead, d );
}

//!Implementation of AbstractStreamServerSocket::listen
bool TCPServerSocket::listen( int queueLen )
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
}

void TCPServerSocket::terminateAsyncIO( bool waitForRunningHandlerCompletion )
{
    //TODO #ak add general implementation to Socket class and remove this method
    if (waitForRunningHandlerCompletion)
    {
        QnWaitCondition cond;
        QnMutex mtx;
        bool cancelled = false;

        m_implDelegate.dispatchImpl(
            [this, &cond, &mtx, &cancelled]()
            {
                aio::AIOService::instance()->cancelPostedCalls(
                    static_cast<Pollable*>(&m_implDelegate), true);
                aio::AIOService::instance()->removeFromWatch(
                    static_cast<Pollable*>(&m_implDelegate), aio::etRead, true);
                aio::AIOService::instance()->removeFromWatch(
                    static_cast<Pollable*>(&m_implDelegate), aio::etTimedOut, true);

                QnMutexLocker lk(&mtx);
                cancelled = true;
                cond.wakeAll();
            } );

        QnMutexLocker lk(&mtx);
        while(!cancelled)
            cond.wait(lk.mutex());
    }
    else
    {
        m_implDelegate.dispatchImpl(
            [this]()
            {
                //m_implDelegate.impl()->terminated.store(true, std::memory_order_relaxed);
                aio::AIOService::instance()->cancelPostedCalls(
                    static_cast<Pollable*>(&m_implDelegate), true);
                aio::AIOService::instance()->removeFromWatch(
                    static_cast<Pollable*>(&m_implDelegate), aio::etRead, true);
                aio::AIOService::instance()->removeFromWatch(
                    static_cast<Pollable*>(&m_implDelegate), aio::etTimedOut, true);
        } );
    }
}

//!Implementation of AbstractStreamServerSocket::accept
AbstractStreamSocket* TCPServerSocket::accept()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(m_implDelegate.impl());

    unsigned int recvTimeoutMs = 0;
    if( !getRecvTimeout( &recvTimeoutMs ) )
        return nullptr;

    return d->accept( recvTimeoutMs );
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
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
    if( ::setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof( buff_size ) )<0 )
    {
        //error
    }
}

void UDPSocket::setBroadcast() {
    // If this fails, we'll hear about it when we try to send.  This will allow
    // system that cannot broadcast to continue if they don't plan to broadcast
    int broadcastPermission = 1;
    setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_BROADCAST,
               (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

bool UDPSocket::sendTo(const void *buffer, int bufferLen)
{
    // Write out the whole buffer as a single message.
    #ifdef _WIN32
        return sendto(
            m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0,
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
                &::sendto, m_implDelegate.handle(), (const void*)buffer, (size_t)bufferLen, flags,
                m_destAddr.ptr.get(), m_destAddr.size),
            sendTimeout) == bufferLen;
    #endif
}

bool UDPSocket::setMulticastTTL(unsigned char multicastTTL)  {
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_MULTICAST_TTL,
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
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_MULTICAST_IF, (raw_type *)&localInterface, sizeof( localInterface ) ) < 0 )
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
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
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
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
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
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
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
    if( setsockopt( m_implDelegate.handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
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
            m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0,
            m_destAddr.ptr.get(), m_destAddr.size);
    #else
        unsigned int sendTimeout = 0;
        if (!getSendTimeout(&sendTimeout))
            return -1;

        return doInterruptableSystemCallWithTimeout<>(
            std::bind(
                &::sendto, m_implDelegate.handle(),
                (const void*)buffer, (size_t)bufferLen, 0,
                m_destAddr.ptr.get(), m_destAddr.size),
            sendTimeout);
    #endif
}

//!Implementation of AbstractDatagramSocket::setDestAddr
bool UDPSocket::setDestAddr( const SocketAddress& foreignEndpoint )
{
    m_destAddr = m_implDelegate.makeAddr(foreignEndpoint);
    return (bool)m_destAddr.ptr;
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
    FD_SET( m_implDelegate.handle(), &read_set );
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
    sockPollfd.fd = m_implDelegate.handle();
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
    int* const sourcePort )
{
    sockaddr_in clntAddr;
    socklen_t addrLen = sizeof( clntAddr );

#ifdef _WIN32
    int rtn = recvfrom( m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0, (sockaddr *)&clntAddr, (socklen_t *)&addrLen );
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int rtn = doInterruptableSystemCallWithTimeout<>(
        std::bind( &::recvfrom, m_implDelegate.handle(), (void*)buffer, (size_t)bufferLen, 0, (sockaddr*)&clntAddr, (socklen_t*)&addrLen ),
        recvTimeout );
#endif

    if( rtn >= 0 )
    {
        *sourceAddress = HostAddress( clntAddr.sin_addr );
        *sourcePort = ntohs(clntAddr.sin_port);
    }
    return rtn;
}
