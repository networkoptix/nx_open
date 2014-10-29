#include "system_socket.h"

#include <memory>
#include <boost/type_traits/is_same.hpp>

#include <atomic>

#include <platform/win32_syscall_resolver.h>
#include <utils/common/systemerror.h>
#include <utils/common/warnings.h>
#include <utils/network/ssl_socket.h>

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
static const int ERR_WOULDBLOCK = EWOULDBLOCK;
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
    return setLocalAddressAndPort( localAddress.address.toString(), localAddress.port );
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
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::getPeerAddress
SocketAddress Socket::getPeerAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::close
void Socket::close()
{
    if( m_fd == -1 )
        return;

#ifdef Q_OS_WIN
    ::shutdown(m_fd, SD_BOTH);
#else
    ::shutdown(m_fd, SHUT_RDWR);
#endif

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
bool Socket::getReuseAddrFlag( bool* val ) const
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
bool Socket::getMtu( unsigned int* mtuValue ) const
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
bool Socket::getSendBufferSize( unsigned int* buffSize ) const
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
bool Socket::getRecvBufferSize( unsigned int* buffSize ) const
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

bool Socket::getLastError( SystemError::ErrorCode* errorCode ) const
{
    socklen_t optLen = sizeof(*errorCode);
    return getsockopt(m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(errorCode), &optLen) == 0;
}

bool Socket::post( std::function<void()>&& handler )
{
    return m_baseAsyncHelper->post( std::move(handler) );
}

bool Socket::dispatch( std::function<void()>&& handler )
{
    return m_baseAsyncHelper->dispatch( std::move(handler) );
}


QString Socket::getLocalHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

QString Socket::getPeerHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

quint32 Socket::getPeerAddressUint() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return 0;

    return ntohl(addr.sin_addr.s_addr);
}

unsigned short Socket::getLocalPort() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return 0;
    }

    return ntohs(addr.sin_port);
}

bool Socket::setLocalPort(unsigned short localPort)  {
    // Bind the socket to its port
    sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(localPort);

    return ::bind(m_fd, (sockaddr *) &localAddr, sizeof(sockaddr_in)) == 0;
}

bool Socket::setLocalAddressAndPort(const QString &localAddress,
                                    unsigned short localPort)  {
    // Get the address of the requested host
    sockaddr_in localAddr;
    if (!fillAddr(localAddress, localPort, localAddr))
        return false;

    return ::bind(m_fd, (sockaddr *) &localAddr, sizeof(localAddr)) == 0;
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
    SystemSocketImpl* impl )
:
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<SystemSocketImpl>(impl) ),
    m_baseAsyncHelper( std::move(asyncHelper) ),
    m_nonBlockingMode( false )
{
    createSocket( type, protocol );
}

Socket::Socket(
    std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>> asyncHelper,
    int _sockDesc,
    SystemSocketImpl* impl )
:
    Pollable(
        _sockDesc,
        std::unique_ptr<SystemSocketImpl>(impl) ),
    m_baseAsyncHelper( std::move(asyncHelper) ),
    m_nonBlockingMode( false )
{
}

Socket::Socket(
    int type,
    int protocol,
    SystemSocketImpl* impl )
:
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<SystemSocketImpl>(impl) ),
    m_baseAsyncHelper( new BaseAsyncSocketImplHelper<Pollable>(this) ),
    m_nonBlockingMode( false )
{
    createSocket( type, protocol );
}

Socket::Socket(
    int _sockDesc,
    SystemSocketImpl* impl )
:
    Pollable(
        _sockDesc,
        std::unique_ptr<SystemSocketImpl>(impl) ),
    m_baseAsyncHelper( new BaseAsyncSocketImplHelper<Pollable>(this) ),
    m_nonBlockingMode( false )
{
}

// Function to fill in address structure given an address and port
bool Socket::fillAddr(const QString &address, unsigned short port,
                     sockaddr_in &addr) {

    memset(&addr, 0, sizeof(addr));  // Zero out address structure
    addr.sin_family = AF_INET;       // Internet address

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow only IPv4 */
    hints.ai_socktype = 0; /* Any socket */
    hints.ai_flags = AI_ALL;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    addrinfo *addressInfo;
    int status = getaddrinfo(address.toLatin1(), 0, &hints, &addressInfo);
    if (status != 0)
        return false;

    addr.sin_addr.s_addr = ((struct sockaddr_in *) (addressInfo->ai_addr))->sin_addr.s_addr;
    addr.sin_port = htons(port);     // Assign port in network byte order

    freeaddrinfo(addressInfo);

    return true;
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

    // Make a new socket
    m_fd = socket(PF_INET, type, protocol);
    if( m_fd < 0 )
        return false;

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
    static const size_t MILLIS_IN_SEC = 1000;
    static const size_t NSECS_IN_MS = 1000000;

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
    SystemSocketImpl* sockImpl )
:
    Socket(
        std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>>(
            new AsyncSocketImplHelper<Pollable>( this, abstractSocketPtr ) ),
        type,
        protocol,
        sockImpl ),
    m_aioHelper( nullptr ),
    m_connected( false )
{
    m_aioHelper = static_cast<AsyncSocketImplHelper<Pollable>*>(this->m_baseAsyncHelper.get());
}

CommunicatingSocket::CommunicatingSocket(
    AbstractCommunicatingSocket* abstractSocketPtr,
    int newConnSD,
    SystemSocketImpl* sockImpl )
:
    Socket(
        std::unique_ptr<BaseAsyncSocketImplHelper<Pollable>>(
            new AsyncSocketImplHelper<Pollable>( this, abstractSocketPtr ) ),
        newConnSD,
        sockImpl ),
    m_aioHelper( nullptr ),
    m_connected( true )   //this constructor is used
{
    m_aioHelper = static_cast<AsyncSocketImplHelper<Pollable>*>(this->m_baseAsyncHelper.get());
}

CommunicatingSocket::~CommunicatingSocket()
{
    m_aioHelper->terminate();
}

void CommunicatingSocket::terminateAsyncIO( bool waitForRunningHandlerCompletion )
{
    m_aioHelper->terminateAsyncIO();
    m_aioHelper->cancelAsyncIO( aio::etNone, waitForRunningHandlerCompletion );
}

//!Implementation of AbstractCommunicatingSocket::connect
bool CommunicatingSocket::connect( const QString& foreignAddress, unsigned short foreignPort, unsigned int timeoutMs )
{
    // Get the address of the requested host
    m_connected = false;

    sockaddr_in destAddr;
    if (!fillAddr(foreignAddress, foreignPort, destAddr))
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if( !getNonBlockingMode( &isNonBlockingModeBak ) )
        return false;
    if( !isNonBlockingModeBak && !setNonBlockingMode( true ) )
        return false;

    int connectResult = ::connect(m_fd, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

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
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        qnWarning("Fetch of foreign address failed (getpeername()).");
        return SocketAddress();
    }
    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
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
#ifdef Q_OS_WIN
    ::shutdown(m_fd, SD_BOTH);
#else
    ::shutdown(m_fd, SHUT_RDWR);
#endif
}

QString CommunicatingSocket::getForeignHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        qnWarning("Fetch of foreign address failed (getpeername()).");
        return QString();
    }
    return QLatin1String(inet_ntoa(addr.sin_addr));
}

unsigned short CommunicatingSocket::getForeignPort() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_fd, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        qWarning()<<"Fetch of foreign port failed (getpeername()). "<<SystemError::getLastOSErrorText();
        return -1;
    }
    return ntohs(addr.sin_port);
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
    public SystemSocketImpl
{
public:
    MIB_TCPROW win32TcpTableRow;

    Win32TcpSocketImpl()
    {
        memset( &win32TcpTableRow, 0, sizeof(win32TcpTableRow) );
    }
};
#endif

TCPSocket::TCPSocket()
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
        )
{
}

TCPSocket::TCPSocket( const QString &foreignAddress, unsigned short foreignPort )
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
        )
{
    connect( foreignAddress, foreignPort, AbstractCommunicatingSocket::DEFAULT_TIMEOUT_MILLIS );
}

TCPSocket::TCPSocket(int newConnSD)
:
    base_type(
        newConnSD
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
bool TCPSocket::getNoDelay( bool* value ) const
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
        return ::accept( m_fd, NULL, NULL );
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
    public SystemSocketImpl
{
public:
    int socketHandle;
    AsyncServerSocketHelper<Pollable> asyncServerSocketHelper;

    TCPServerSocketPrivate( Socket* sock, AbstractStreamServerSocket* abstractSock )
    :
        socketHandle( -1 ),
        asyncServerSocketHelper( sock, abstractSock )
    {
    }

    AbstractStreamSocket* accept( unsigned int recvTimeoutMs )
    {
        int newConnSD = acceptWithTimeout( socketHandle, recvTimeoutMs );
        if( newConnSD >= 0 )
        {
            return new TCPSocket(newConnSD);
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

TCPServerSocket::TCPServerSocket()
:
    base_type(
        SOCK_STREAM,
        IPPROTO_TCP,
        new TCPServerSocketPrivate( &m_implDelegate, this ) )
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
    return d->asyncServerSocketHelper.acceptAsync( std::move(handler) );
}

//!Implementation of AbstractStreamServerSocket::listen
bool TCPServerSocket::listen( int queueLen )
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
}

void TCPServerSocket::terminateAsyncIO( bool /*waitForRunningHandlerCompletion*/ )
{
    //m_implDelegate.m_baseAsyncHelper->terminateAsyncIO();
    m_implDelegate.impl()->terminated.store( true, std::memory_order_relaxed );
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

void TCPServerSocket::cancelAsyncIO( bool waitForRunningHandlerCompletion )
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(m_implDelegate.impl());
    d->asyncServerSocketHelper.cancelAsyncIO(waitForRunningHandlerCompletion);
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
}


//////////////////////////////////////////////////////////
///////// class UDPSocket
//////////////////////////////////////////////////////////

// UDPSocket Code

UDPSocket::UDPSocket()
:
    base_type(SOCK_DGRAM, IPPROTO_UDP)
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );
    setBroadcast();
    int buff_size = 1024*512;
    if( ::setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof( buff_size ) )<0 )
    {
        //error
    }
}

UDPSocket::UDPSocket(unsigned short localPort)
:
    base_type( SOCK_DGRAM, IPPROTO_UDP )
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );
    m_implDelegate.setLocalPort( localPort );
    setBroadcast();

    int buff_size = 1024*512;
    if( ::setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof( buff_size ) )<0 )
    {
        //error
    }
}

UDPSocket::UDPSocket(const QString &localAddress, unsigned short localPort)
:
    base_type( SOCK_DGRAM, IPPROTO_UDP )
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );

    if( !m_implDelegate.setLocalAddressAndPort( localAddress, localPort ) )
    {
        qWarning() << "Can't create UDP socket: " << SystemError::getLastOSErrorText();
        return;
    }

    setBroadcast();
    int buff_size = 1024*512;
    if( ::setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof( buff_size ) )<0 )
    {
        //TODO #ak
    }
}

void UDPSocket::setBroadcast() {
    // If this fails, we'll hear about it when we try to send.  This will allow
    // system that cannot broadcast to continue if they don't plan to broadcast
    int broadcastPermission = 1;
    setsockopt( m_implDelegate.handle(), SOL_SOCKET, SO_BROADCAST,
               (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

void UDPSocket::setDestPort(unsigned short foreignPort)
{
    m_destAddr.sin_port = htons(foreignPort);
}


bool UDPSocket::sendTo(const void *buffer, int bufferLen)
{
    // Write out the whole buffer as a single message.

#ifdef _WIN32
    return sendto( m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0,
               (sockaddr *) &m_destAddr, sizeof(m_destAddr)) == bufferLen;
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        std::bind(&::sendto, m_implDelegate.handle(), (const void*)buffer, (size_t)bufferLen,
#ifdef __linux__
            MSG_NOSIGNAL,
#else
            0,
#endif
            (const sockaddr *) &m_destAddr, (socklen_t)sizeof(m_destAddr)),
        sendTimeout ) == bufferLen;
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
            qWarning() << "failed to join multicast group" << multicastGroup << "from IF" << multicastIF;
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
    return sendto( m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0,
               (sockaddr *) &m_destAddr, sizeof(m_destAddr));
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        std::bind(&::sendto, m_implDelegate.handle(), (const void*)buffer, (size_t)bufferLen, 0, (const sockaddr *) &m_destAddr, (socklen_t)sizeof(m_destAddr)),
        sendTimeout );
#endif
}

//!Implementation of AbstractDatagramSocket::setDestAddr
bool UDPSocket::setDestAddr( const QString& foreignAddress, unsigned short foreignPort )
{
    return m_implDelegate.fillAddr( foreignAddress, foreignPort, m_destAddr );
}

//!Implementation of AbstractDatagramSocket::sendTo
bool UDPSocket::sendTo(
    const void* buffer,
    unsigned int bufferLen,
    const SocketAddress& foreignAddress )
{
    setDestAddr( foreignAddress.address.toString(), foreignAddress.port );  //TODO #ak optimize, remove (to QString); (from QString) operations
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
    QString& sourceAddress,
    unsigned short &sourcePort )
{
    int rtn = recvFrom(
        buffer,
        bufferLen,
        &m_prevDatagramAddress.address,
        &m_prevDatagramAddress.port );

    if (rtn >= 0) {
        sourceAddress = QLatin1String(inet_ntoa(m_prevDatagramAddress.address.inAddr()));
        sourcePort = m_prevDatagramAddress.port;
    }
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
