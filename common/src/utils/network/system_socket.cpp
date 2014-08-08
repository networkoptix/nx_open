#include "system_socket.h"

#include <boost/type_traits/is_same.hpp>

#include <utils/common/warnings.h>
#include <utils/common/systemerror.h>
#include <utils/network/ssl_socket.h>

#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#endif

#include <QtCore/QElapsedTimer>

#include "aio/aioservice.h"
#include "system_socket_impl.h"


#ifdef Q_OS_WIN
/* Check that the typedef in AbstractSocket is correct. */
static_assert(boost::is_same<AbstractSocket::SOCKET_HANDLE, SOCKET>::value, "Invalid socket type is used in AbstractSocket.");
typedef int socklen_t;
typedef char raw_type;       // Type used for raw data on this platform
#else
#include <poll.h>
//#include <sys/select.h>
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

//TODO/IMPL set prevErrorCode to noError in case of success

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

// SocketException Code

//////////////////////////////////////////////////////////
// SocketException implementation
//////////////////////////////////////////////////////////

SocketException::SocketException(const QString &message, bool inclSysMsg)
throw() {
    m_message[0] = 0;

    QString userMessage(message);
    if (inclSysMsg) {
        userMessage.append(QLatin1String(": "));
        userMessage.append(QLatin1String(strerror(errno)));
    }

    QByteArray data = userMessage.toLatin1();
    strncpy(m_message, data.data(), MAX_ERROR_MSG_LENGTH-1);
    m_message[MAX_ERROR_MSG_LENGTH-1] = 0;
}

SocketException::~SocketException() throw() {
}

const char *SocketException::what() const throw() {
    return m_message;
}


//////////////////////////////////////////////////////////
// Socket implementation
//////////////////////////////////////////////////////////

Socket::~Socket() {
    close();
    delete m_impl;
    m_impl = NULL;
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

    if (getsockname(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::getPeerAddress
SocketAddress Socket::getPeerAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::close
void Socket::close()
{
    if( m_socketHandle == -1 )
        return;

    //checking that socket is not registered in aio
    //assert( !aio::AIOService::instance()->isSocketBeingWatched(this) );

#ifdef Q_OS_WIN
    ::shutdown(m_socketHandle, SD_BOTH);
#else
    ::shutdown(m_socketHandle, SHUT_RDWR);
#endif

#ifdef WIN32
    ::closesocket(m_socketHandle);
#else
    ::close(m_socketHandle);
#endif
    m_socketHandle = -1;
}

bool Socket::isClosed() const
{
    return m_socketHandle == -1;
}

//!Implementation of AbstractSocket::setReuseAddrFlag
bool Socket::setReuseAddrFlag( bool reuseAddr )
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(m_socketHandle, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
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

    if (::getsockopt(m_socketHandle, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
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
    if( ioctlsocket( m_socketHandle, FIONBIO, &_val ) == 0 )
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#else
    long currentFlags = fcntl( m_socketHandle, F_GETFL, 0 );
    if( currentFlags == -1 )
        return false;
    if( val )
        currentFlags |= O_NONBLOCK;
    else
        currentFlags &= ~O_NONBLOCK;
    if( fcntl( m_socketHandle, F_SETFL, currentFlags ) == 0 )
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
    return ::getsockopt(m_socketHandle, IPPROTO_IP, IP_MTU, (char*)mtuValue, &optLen) == 0;
#else
    *mtuValue = 1500;   //in winsock there is no IP_MTU, returning 1500 as most common value
    return true;
#endif
}

//!Implementation of AbstractSocket::setSendBufferSize
bool Socket::setSendBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_socketHandle, SOL_SOCKET, SO_SNDBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getSendBufferSize
bool Socket::getSendBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = 0;
    return ::getsockopt(m_socketHandle, SOL_SOCKET, SO_SNDBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvBufferSize
bool Socket::setRecvBufferSize( unsigned int buff_size )
{
    return ::setsockopt(m_socketHandle, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getRecvBufferSize
bool Socket::getRecvBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = 0;
    return ::getsockopt(m_socketHandle, SOL_SOCKET, SO_RCVBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvTimeout
bool Socket::setRecvTimeout( unsigned int ms )
{
    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (m_socketHandle, SOL_SOCKET, SO_RCVTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(m_socketHandle, SOL_SOCKET, SO_RCVTIMEO,(const void *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<m_socketHandle<<"). setRecvTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
        return false;
    }
    m_readTimeoutMS = ms;
    return true;
}

//!Implementation of AbstractSocket::getRecvTimeout
bool Socket::getRecvTimeout( unsigned int* millis )
{
    *millis = m_readTimeoutMS;
    return true;
}

//!Implementation of AbstractSocket::setSendTimeout
bool Socket::setSendTimeout( unsigned int ms )
{
    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (m_socketHandle, SOL_SOCKET, SO_SNDTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(m_socketHandle, SOL_SOCKET, SO_SNDTIMEO,(const char *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<m_socketHandle<<"). setSendTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
        return false;
    }
    m_writeTimeoutMS = ms;
    return true;
}

//!Implementation of AbstractSocket::getSendTimeout
bool Socket::getSendTimeout( unsigned int* millis )
{
    *millis = m_writeTimeoutMS;
    return true;
}

bool Socket::getLastError( SystemError::ErrorCode* errorCode )
{
    socklen_t optLen = sizeof(*errorCode);
    return getsockopt(m_socketHandle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(errorCode), &optLen) == 0;
}

AbstractSocket::SOCKET_HANDLE Socket::handle() const
{
    return m_socketHandle;
}


QString Socket::getLocalHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

QString Socket::getPeerHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

quint32 Socket::getPeerAddressUint() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return 0;

    return ntohl(addr.sin_addr.s_addr);
}

unsigned short Socket::getLocalPort() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
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

    return ::bind(m_socketHandle, (sockaddr *) &localAddr, sizeof(sockaddr_in)) == 0;
}

bool Socket::setLocalAddressAndPort(const QString &localAddress,
                                    unsigned short localPort)  {
    // Get the address of the requested host
    sockaddr_in localAddr;
    if (!fillAddr(localAddress, localPort, localAddr))
        return false;

    return ::bind(m_socketHandle, (sockaddr *) &localAddr, sizeof(localAddr)) == 0;
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

SocketImpl* Socket::impl()
{
    return m_impl;
}

const SocketImpl* Socket::impl() const
{
    return m_impl;
}

Socket::Socket(int type, int protocol)
:
    m_socketHandle( -1 ),
    m_impl( NULL ),
    m_nonBlockingMode( false ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    createSocket( type, protocol );

    m_impl = new SocketImpl();
}

Socket::Socket(int _sockDesc)
:
    m_socketHandle( -1 ),
    m_impl( NULL ),
    m_nonBlockingMode( false ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    this->m_socketHandle = _sockDesc;
    m_impl = new SocketImpl();
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
    if (status != 0) {
#ifdef UNICODE
        QString errorMessage = QString::fromWCharArray(gai_strerror(status));
#else
        QString errorMessage = QString::fromLocal8Bit(gai_strerror(status));
#endif  /* UNICODE */

        return false;
    }

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
    m_socketHandle = socket(PF_INET, type, protocol);
    if( m_socketHandle < 0 )
        return false;

#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(m_socketHandle, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
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

CommunicatingSocket::CommunicatingSocket(int type, int protocol)
    : Socket(type, protocol),
      m_connected(false)
{
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) 
    : Socket(newConnSD),
      m_connected(true)
{
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

    int connectResult = ::connect(m_socketHandle, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

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
    FD_SET(m_socketHandle, &wrtFDS);

    /* set timeout values */
    timeVal.tv_sec  = timeoutMs/1000;
    timeVal.tv_usec = timeoutMs%1000;
    iSelRet = ::select(
        m_socketHandle + 1,
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
        sockPollfd.fd = m_socketHandle;
        sockPollfd.events = POLLOUT;
#ifdef _GNU_SOURCE
        sockPollfd.events |= POLLRDHUP;
#endif
        iSelRet = ::poll( &sockPollfd, 1, timeoutMs );


        //timeVal.tv_sec  = timeoutMs/1000;
        //timeVal.tv_usec = timeoutMs%1000;

        //iSelRet = ::select( m_socketHandle + 1, NULL, &wrtFDS, NULL, timeoutMs >= 0 ? &timeVal : NULL );
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
    int bytesRead = ::recv(m_socketHandle, (raw_type *) buffer, bufferLen, flags);
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::recv, m_socketHandle, (void*)buffer, (size_t)bufferLen, flags),
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
    int sended = ::send(m_socketHandle, (raw_type*) buffer, bufferLen, 0);
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    int sended = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::send, m_socketHandle, (const void*)buffer, (size_t)bufferLen,
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
const SocketAddress CommunicatingSocket::getForeignAddress()
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
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
    Socket::close();
    m_connected = false;
}

void CommunicatingSocket::shutdown()
{
#ifdef Q_OS_WIN
    ::shutdown(m_socketHandle, SD_BOTH);
#else
    ::shutdown(m_socketHandle, SHUT_RDWR);
#endif
}

QString CommunicatingSocket::getForeignHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        qnWarning("Fetch of foreign address failed (getpeername()).");
        return QString();
    }
    return QLatin1String(inet_ntoa(addr.sin_addr));
}

unsigned short CommunicatingSocket::getForeignPort() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(m_socketHandle, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        qWarning()<<"Fetch of foreign port failed (getpeername()). "<<SystemError::getLastOSErrorText();
        return -1;
    }
    return ntohs(addr.sin_port);
}


//////////////////////////////////////////////////////////
///////// class TCPSocket
//////////////////////////////////////////////////////////

// TCPSocket Code

TCPSocket::TCPSocket()
:
    base_type( SOCK_STREAM, IPPROTO_TCP )
{
}

TCPSocket::TCPSocket( const QString &foreignAddress, unsigned short foreignPort )
:
    base_type( SOCK_STREAM, IPPROTO_TCP )
{
    connect( foreignAddress, foreignPort, AbstractCommunicatingSocket::DEFAULT_TIMEOUT_MILLIS );
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

TCPSocket::TCPSocket( int newConnSD )
:
    base_type( newConnSD )
{
}


//////////////////////////////////////////////////////////
///////// class TCPServerSocket
//////////////////////////////////////////////////////////

// TCPServerSocket Code

static const int DEFAULT_ACCEPT_TIMEOUT_MSEC = 250;
/*! 
    \return fd (>=0) on success, <0 on error (-2 if timed out)
*/
static int acceptWithTimeout( int m_socketHandle, int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC )
{
    int result = 0;

#ifdef _WIN32
    fd_set read_set;
    FD_ZERO( &read_set );
    FD_SET( m_socketHandle, &read_set );

    fd_set except_set;
    FD_ZERO( &except_set );
    FD_SET( m_socketHandle, &except_set );

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeoutMillis * 1000;

    result = ::select( m_socketHandle + 1, &read_set, NULL, &except_set, &timeout );
    if( result < 0 )
        return result;
    if( result == 0 )   //timeout
    {
        ::SetLastError( SystemError::timedOut );
        return -1;
    }
    if( FD_ISSET( m_socketHandle, &except_set ) )
    {
        int errorCode = 0;
        int errorCodeLen = sizeof( errorCode );
        if( getsockopt( m_socketHandle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &errorCodeLen ) != 0 )
            return -1;
        ::SetLastError( errorCode );
        return -1;
    }
    return ::accept( m_socketHandle, NULL, NULL );
#else
    struct pollfd sockPollfd;
    memset( &sockPollfd, 0, sizeof(sockPollfd) );
    sockPollfd.fd = m_socketHandle;
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
        return ::accept( m_socketHandle, NULL, NULL );
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
        if( getsockopt( m_socketHandle, SOL_SOCKET, SO_ERROR, &errorCode, &errorCodeLen ) != 0 )
            return -1;
        errno = errorCode;
        return -1;
    }
    return -1;
#endif
}



TCPServerSocket::TCPServerSocket()
:
    base_type( SOCK_STREAM, IPPROTO_TCP )
{
    setRecvTimeout( DEFAULT_ACCEPT_TIMEOUT_MSEC );
}

int TCPServerSocket::accept(int sockDesc)
{
    return acceptWithTimeout( sockDesc );
}

//!Implementation of AbstractStreamServerSocket::listen
bool TCPServerSocket::listen( int queueLen )
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
}

//!Implementation of AbstractStreamServerSocket::accept
AbstractStreamSocket* TCPServerSocket::accept()
{
    unsigned int recvTimeoutMs = 0;
    if( !getRecvTimeout( &recvTimeoutMs ) )
        return NULL;
    const int newConnSD = acceptWithTimeout( m_implDelegate.handle(), recvTimeoutMs );
    return newConnSD >= 0 ? new TCPSocket( newConnSD ) : nullptr;
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen( m_implDelegate.handle(), queueLen ) == 0;
}

// -------------------------- TCPSslServerSocket ----------------

#ifdef ENABLE_SSL
TCPSslServerSocket::TCPSslServerSocket(bool allowNonSecureConnect): TCPServerSocket(), m_allowNonSecureConnect(allowNonSecureConnect)
{

}

AbstractStreamSocket* TCPSslServerSocket::accept()
{
    AbstractStreamSocket* sock = TCPServerSocket::accept();
    if (!sock)
        return 0;

    if (m_allowNonSecureConnect)
        return new QnMixedSSLSocket(sock);

    else
        return new QnSSLSocket(sock, true);

#if 0
    // transparent accept required state machine here. doesn't implemented. Handshake implemented on first IO operations

    QnSSLSocket* sslSock = new QnSSLSocket(sock);
    if (sslSock->doServerHandshake())
        return sslSock;
    
    delete sslSock;
    return 0;
#endif
}
#endif // ENABLE_SSL

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

//void UDPSocket::disconnect()  {
//    sockaddr_in nullAddr;
//    memset(&nullAddr, 0, sizeof(nullAddr));
//    nullAddr.sin_family = AF_UNSPEC;
//
//    // Try to disconnect
//    if (::connect(m_socketHandle, (sockaddr *) &nullAddr, sizeof(nullAddr)) < 0) {
//#ifdef WIN32
//        if (errno != WSAEAFNOSUPPORT)
//#else
//        if (errno != EAFNOSUPPORT)
//#endif
//        {
//            throw SocketException(tr("Disconnect failed (connect())."), true);
//        }
//    }
//}

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
    const QString& foreignAddress,
    unsigned short foreignPort )
{
    setDestAddr( foreignAddress, foreignPort );
    return sendTo( buffer, bufferLen );
}

int UDPSocket::recvFrom(
    void *buffer,
    int bufferLen,
    QString& sourceAddress,
    unsigned short &sourcePort )
{
    sockaddr_in clntAddr;
    socklen_t addrLen = sizeof(clntAddr);

#ifdef _WIN32
    int rtn = recvfrom( m_implDelegate.handle(), (raw_type *)buffer, bufferLen, 0, (sockaddr *)&clntAddr, (socklen_t *)&addrLen );
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int rtn = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::recvfrom, m_socketHandle, (void*)buffer, (size_t)bufferLen, 0, (sockaddr*)&clntAddr, (socklen_t*)&addrLen),
        recvTimeout );
#endif

    if (rtn >= 0) {
        sourceAddress = QLatin1String(inet_ntoa(clntAddr.sin_addr));
        sourcePort = ntohs(clntAddr.sin_port);
    }
    return rtn;
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
    sockPollfd.fd = m_socketHandle;
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    return (::poll( &sockPollfd, 1, 0 ) == 1) && ((sockPollfd.revents & POLLIN) != 0);
#endif
}
