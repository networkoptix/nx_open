#include "system_socket.h"

#include <boost/type_traits/is_same.hpp>

#include <utils/common/warnings.h>
#include <utils/common/stdext.h>
#include <utils/network/ssl_socket.h>

#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#endif

#include <QtCore/QElapsedTimer>

#include "system_socket_impl.h"
#include <utils/common/systemerror.h>


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
//    if( !res )
//    {
//        saveErrorInfo();
//        setStatusBit( Socket::sbFailed );
//    }
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

    if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::getPeerAddress
SocketAddress Socket::getPeerAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return SocketAddress();

    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractSocket::close
void Socket::close()
{
    if( sockDesc == -1 )
        return;

#ifdef Q_OS_WIN
    ::shutdown(sockDesc, SD_BOTH);
#else
    ::shutdown(sockDesc, SHUT_RDWR);
#endif

#ifdef WIN32
    ::closesocket(sockDesc);
#else
    ::close(sockDesc);
#endif
    sockDesc = -1;
}

bool Socket::isClosed() const
{
    return sockDesc == -1;
}

//!Implementation of AbstractSocket::setReuseAddrFlag
bool Socket::setReuseAddrFlag( bool reuseAddr )
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
        m_prevErrorCode = SystemError::getLastOSErrorCode();
        m_lastError = SystemError::getLastOSErrorText();
        qnWarning("Can't set SO_REUSEADDR flag to socket: %1.", strerror(errno));
        return false;
    }
    return true;
}

//!Implementation of AbstractSocket::reuseAddrFlag
bool Socket::getReuseAddrFlag( bool* val )
{
    int reuseAddrVal = 0;
    socklen_t optLen = 0;

    if (::getsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
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
    if( ioctlsocket( sockDesc, FIONBIO, &_val ) == 0 )
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#else
    long currentFlags = fcntl( sockDesc, F_GETFL, 0 );
    if( currentFlags == -1 )
        return false;
    if( val )
        currentFlags |= O_NONBLOCK;
    else
        currentFlags &= ~O_NONBLOCK;
    if( fcntl( sockDesc, F_SETFL, currentFlags ) == 0 )
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
    return ::getsockopt(sockDesc, IPPROTO_IP, IP_MTU, (char*)mtuValue, &optLen) == 0;
#else
    *mtuValue = 1500;   //in winsock there is no IP_MTU, returning 1500 as most common value
    return true;
#endif
}

//!Implementation of AbstractSocket::setSendBufferSize
bool Socket::setSendBufferSize( unsigned int buff_size )
{
    return ::setsockopt(sockDesc, SOL_SOCKET, SO_SNDBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getSendBufferSize
bool Socket::getSendBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = 0;
    return ::getsockopt(sockDesc, SOL_SOCKET, SO_SNDBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvBufferSize
bool Socket::setRecvBufferSize( unsigned int buff_size )
{
    return ::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

//!Implementation of AbstractSocket::getRecvBufferSize
bool Socket::getRecvBufferSize( unsigned int* buffSize )
{
    socklen_t optLen = 0;
    return ::getsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (char*)buffSize, &optLen) == 0;
}

//!Implementation of AbstractSocket::setRecvTimeout
bool Socket::setRecvTimeout( unsigned int ms )
{
    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (sockDesc, SOL_SOCKET, SO_RCVTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVTIMEO,(const void *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<sockDesc<<"). setRecvTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
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
    if ( setsockopt (sockDesc, SOL_SOCKET, SO_SNDTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_SNDTIMEO,(const char *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        qWarning()<<"handle("<<sockDesc<<"). setSendTimeout("<<ms<<") failed. "<<SystemError::getLastOSErrorText();
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

AbstractSocket::SOCKET_HANDLE Socket::handle() const
{
    return sockDesc;
}


QString Socket::lastError() const
{
    return m_lastError;
}

QString Socket::getLocalHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

QString Socket::getPeerHostAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

quint32 Socket::getPeerAddressUint() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
        return 0;

    return ntohl(addr.sin_addr.s_addr);
}

unsigned short Socket::getLocalPort() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
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

    return ::bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) == 0;
}

bool Socket::setLocalAddressAndPort(const QString &localAddress,
                                    unsigned short localPort)  {
    m_lastError.clear();

    // Get the address of the requested host
    sockaddr_in localAddr;
    if (!fillAddr(localAddress, localPort, localAddr))
        return false;

    return ::bind(sockDesc, (sockaddr *) &localAddr, sizeof(localAddr)) == 0;
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

bool Socket::failed() const
{
    return (m_status & sbFailed) != 0;
}

SystemError::ErrorCode Socket::prevErrorCode() const
{
    return m_prevErrorCode;
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
    sockDesc( -1 ),
    m_impl( NULL ),
    m_nonBlockingMode( false ),
    m_status( 0 ),
    m_prevErrorCode( SystemError::noError ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    if( !createSocket(type, protocol) )
    {
        saveErrorInfo();
        setStatusBit( sbFailed );
    }

    m_impl = new SocketImpl();
}

Socket::Socket(int _sockDesc)
:
    sockDesc( -1 ),
    m_impl( NULL ),
    m_nonBlockingMode( false ),
    m_status( 0 ),
    m_prevErrorCode( SystemError::noError ),
    m_readTimeoutMS( 0 ),
    m_writeTimeoutMS( 0 )
{
    this->sockDesc = _sockDesc;
    m_impl = new SocketImpl();
}

// Function to fill in address structure given an address and port
bool Socket::fillAddr(const QString &address, unsigned short port,
                     sockaddr_in &addr) {

    m_lastError.clear();
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

        m_lastError = tr("Couldn't resolve %1: %2.").arg(address).arg(errorMessage);
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
    //return (sockDesc = socket(PF_INET, type, protocol)) > 0;
    sockDesc = socket(PF_INET, type, protocol);
    if( sockDesc < 0 )
        return false;

#ifdef SO_NOSIGPIPE
    int val = 1;
    setsockopt(sockDesc, SOL_SOCKET, SO_NOSIGPIPE, (void *)&val, sizeof(val));
#endif
    return true;
}


void Socket::setStatusBit( StatusBit _status )
{
    m_status |= _status;
}

void Socket::clearStatusBit( StatusBit _status )
{
    m_status &= ~_status;
}

void Socket::saveErrorInfo()
{
    m_prevErrorCode = SystemError::getLastOSErrorCode();
    m_lastError = SystemError::toString(m_prevErrorCode);
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
      mConnected(false)
{
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) 
    : Socket(newConnSD),
      mConnected(true)
{
}



//!Implementation of AbstractCommunicatingSocket::connect
bool CommunicatingSocket::connect( const QString& foreignAddress, unsigned short foreignPort, unsigned int timeoutMs )
{
    m_lastError.clear();

    // Get the address of the requested host
    mConnected = false;

    sockaddr_in destAddr;
    if (!fillAddr(foreignAddress, foreignPort, destAddr))
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if( !getNonBlockingMode(&isNonBlockingModeBak) )
        return false;
    if( !isNonBlockingModeBak && !setNonBlockingMode( true ) )
        return false;

    int connectResult = ::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

    if( connectResult != 0 )
    {
        if( SystemError::getLastOSErrorCode() != SystemError::inProgress )
        {
            m_lastError = tr("Couldn't connect to %1: %2.").arg(foreignAddress).arg(SystemError::getLastOSErrorText());
            return false;
        }
        if( isNonBlockingModeBak )
            return true;        //async connect started
    }

    int iSelRet = 0;

#ifdef _WIN32
    timeval timeVal;
    fd_set wrtFDS;

    /* monitor for incomming connections */
    FD_ZERO(&wrtFDS);
    FD_SET(sockDesc, &wrtFDS);

    /* set timeout values */
    timeVal.tv_sec  = timeoutMs/1000;
    timeVal.tv_usec = timeoutMs%1000;
    iSelRet = ::select(
        sockDesc + 1,
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
        sockPollfd.fd = sockDesc;
        sockPollfd.events = POLLOUT;
#ifdef _GNU_SOURCE
        sockPollfd.events |= POLLRDHUP;
#endif
        iSelRet = ::poll( &sockPollfd, 1, timeoutMs );


        //timeVal.tv_sec  = timeoutMs/1000;
        //timeVal.tv_usec = timeoutMs%1000;

        //iSelRet = ::select( sockDesc + 1, NULL, &wrtFDS, NULL, timeoutMs >= 0 ? &timeVal : NULL );
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

    mConnected = iSelRet > 0;

    //restoring original mode
    setNonBlockingMode( isNonBlockingModeBak );
    return mConnected;
}

//!Implementation of AbstractCommunicatingSocket::recv
int CommunicatingSocket::recv( void* buffer, unsigned int bufferLen, int flags )
{
#ifdef _WIN32
    int bytesRead = ::recv(sockDesc, (raw_type *) buffer, bufferLen, flags);
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::recv, sockDesc, (void*)buffer, (size_t)bufferLen, flags),
        recvTimeout );
#endif
    if (bytesRead < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (errCode != SystemError::timedOut && errCode != SystemError::wouldBlock)
            mConnected = false;
    }
    else if (bytesRead == 0)
        mConnected = false; //connection closed by remote host
    return bytesRead;
}

//!Implementation of AbstractCommunicatingSocket::send
int CommunicatingSocket::send( const void* buffer, unsigned int bufferLen )
{
#ifdef _WIN32
    int sended = ::send(sockDesc, (raw_type*) buffer, bufferLen, 0);
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    int sended = doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::send, sockDesc, (const void*)buffer, (size_t)bufferLen, 0),
        sendTimeout );
#endif
    if (sended < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (errCode != SystemError::timedOut && errCode != SystemError::wouldBlock)
            mConnected = false;
    }
    else if (sended == 0)
        mConnected = false;
    return sended;
}

//!Implementation of AbstractCommunicatingSocket::getForeignAddress
const SocketAddress CommunicatingSocket::getForeignAddress()
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        qnWarning("Fetch of foreign address failed (getpeername()).");
        return SocketAddress();
    }
    return SocketAddress( addr.sin_addr, ntohs(addr.sin_port) );
}

//!Implementation of AbstractCommunicatingSocket::isConnected
bool CommunicatingSocket::isConnected() const
{
    return mConnected;
}



void CommunicatingSocket::close()
{
    Socket::close();
    mConnected = false;
}

void CommunicatingSocket::shutdown()
{
#ifdef Q_OS_WIN
    ::shutdown(sockDesc, SD_BOTH);
#else
    ::shutdown(sockDesc, SHUT_RDWR);
#endif
}

QString CommunicatingSocket::getForeignHostAddress()
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        qnWarning("Fetch of foreign address failed (getpeername()).");
        return QString();
    }
    return QLatin1String(inet_ntoa(addr.sin_addr));
}

unsigned short CommunicatingSocket::getForeignPort()  {
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
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
    : CommunicatingSocket(SOCK_STREAM,
                          IPPROTO_TCP) {
}

TCPSocket::TCPSocket( const QString &foreignAddress, unsigned short foreignPort )
:
    CommunicatingSocket(SOCK_STREAM, IPPROTO_TCP)
{
    connect( foreignAddress, foreignPort, AbstractCommunicatingSocket::DEFAULT_TIMEOUT_MILLIS );
}

bool TCPSocket::reopen()
{
    close();
    if( createSocket(SOCK_STREAM, IPPROTO_TCP) ) {
        clearStatusBit( Socket::sbFailed );
        return true;
    }
    saveErrorInfo();
    setStatusBit( Socket::sbFailed );
    return false;
}

bool TCPSocket::setNoDelay( bool value )
{
    int flag = value ? 1 : 0;
    return setsockopt(sockDesc,            // socket affected
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
    if( getsockopt(sockDesc,            // socket affected
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

TCPSocket::TCPSocket(int newConnSD) : CommunicatingSocket(newConnSD) {
}


//////////////////////////////////////////////////////////
///////// class TCPServerSocket
//////////////////////////////////////////////////////////

// TCPServerSocket Code

static const int DEFAULT_ACCEPT_TIMEOUT_MSEC = 250;
/*! 
    \return fd (>=0) on success, <0 on error (-2 if timed out)
*/
static int acceptWithTimeout( int sockDesc, int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC )
{
    int result = 0;

#ifdef _WIN32
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(sockDesc, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = timeoutMillis * 1000;

    result = ::select(sockDesc + 1, &read_set, NULL, NULL, &timeout);
#else
    struct pollfd sockPollfd;
    memset( &sockPollfd, 0, sizeof(sockPollfd) );
    sockPollfd.fd = sockDesc;
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    result = ::poll( &sockPollfd, 1, timeoutMillis );
    if( result == 1 && (sockPollfd.revents & POLLIN) == 0 )
        result = 0;
#endif

    if( result == 0 )
        return -2;  //timed out
    else if( result < 0 )
        return -1;
    return ::accept(sockDesc, NULL, NULL);
}



TCPServerSocket::TCPServerSocket()
:
    Socket(SOCK_STREAM, IPPROTO_TCP)
{
    setRecvTimeout( DEFAULT_ACCEPT_TIMEOUT_MSEC );
}

int TCPServerSocket::accept(int sockDesc)
{
    int result = acceptWithTimeout( sockDesc );
    return result == -2 ? -1 : result;
}

//!Implementation of AbstractStreamServerSocket::listen
bool TCPServerSocket::listen( int queueLen )
{
    return ::listen( sockDesc, queueLen ) == 0;
}

//!Implementation of AbstractStreamServerSocket::accept
AbstractStreamSocket* TCPServerSocket::accept()
{
    unsigned int recvTimeoutMs = 0;
    if( !getRecvTimeout( &recvTimeoutMs ) )
        return NULL;
    int newConnSD = acceptWithTimeout( sockDesc, recvTimeoutMs );
    if( newConnSD >= 0 )
    {
        //clearStatusBit( Socket::sbFailed );
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
        return NULL;    //timeout
    }
    else
    {
        //error
        //saveErrorInfo();
        //setStatusBit( Socket::sbFailed );
        return NULL;
    }
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen(sockDesc, queueLen) == 0;
}

// -------------------------- TCPSslServerSocket ----------------

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

//////////////////////////////////////////////////////////
///////// class UDPSocket
//////////////////////////////////////////////////////////

// UDPSocket Code

UDPSocket::UDPSocket()
:
    CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP)
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );
    setBroadcast();
    int buff_size = 1024*512;
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
    {
        //error
    }
}

UDPSocket::UDPSocket(unsigned short localPort)
:
    CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP)
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );
    setLocalPort(localPort);
    setBroadcast();

    int buff_size = 1024*512;
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
    {
        //error
    }
}

UDPSocket::UDPSocket(const QString &localAddress, unsigned short localPort)
:
    CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP)
{
    memset( &m_destAddr, 0, sizeof(m_destAddr) );

    if (!setLocalAddressAndPort(localAddress, localPort))
    {
        saveErrorInfo();
        setStatusBit(Socket::sbFailed);
        qWarning() << "Can't create UDP socket: " << m_lastError;
        return;
    }

    setBroadcast();
    int buff_size = 1024*512;
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
    {
        saveErrorInfo();
        setStatusBit( Socket::sbFailed );
    }
}

void UDPSocket::setBroadcast() {
    // If this fails, we'll hear about it when we try to send.  This will allow
    // system that cannot broadcast to continue if they don't plan to broadcast
    int broadcastPermission = 1;
    setsockopt(sockDesc, SOL_SOCKET, SO_BROADCAST,
               (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

//void UDPSocket::disconnect()  {
//    sockaddr_in nullAddr;
//    memset(&nullAddr, 0, sizeof(nullAddr));
//    nullAddr.sin_family = AF_UNSPEC;
//
//    // Try to disconnect
//    if (::connect(sockDesc, (sockaddr *) &nullAddr, sizeof(nullAddr)) < 0) {
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
    return sendto(sockDesc, (raw_type *) buffer, bufferLen, 0,
               (sockaddr *) &m_destAddr, sizeof(m_destAddr)) == bufferLen;
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::sendto, sockDesc, (const void*)buffer, (size_t)bufferLen, 0, (const sockaddr *) &m_destAddr, (socklen_t)sizeof(m_destAddr)),
        sendTimeout ) == bufferLen;
#endif
}

bool UDPSocket::setMulticastTTL(unsigned char multicastTTL)  {
    if (setsockopt(sockDesc, IPPROTO_IP, IP_MULTICAST_TTL,
                   (raw_type *) &multicastTTL, sizeof(multicastTTL)) < 0) {
        qnWarning("Multicast TTL set failed (setsockopt()).");
        return false;
    }
    return true;
}

bool UDPSocket::setMulticastIF(const QString& multicastIF)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(multicastIF.toLocal8Bit().data());
    if (setsockopt(sockDesc, IPPROTO_IP, IP_MULTICAST_IF, (raw_type *) &localInterface, sizeof(localInterface)) < 0) 
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
    if (setsockopt(sockDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP,
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
    if (setsockopt(sockDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP,
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
    if (setsockopt(sockDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP,
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
    if (setsockopt(sockDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP,
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
    return sendto(sockDesc, (raw_type *) buffer, bufferLen, 0,
               (sockaddr *) &m_destAddr, sizeof(m_destAddr));
#else
    unsigned int sendTimeout = 0;
    if( !getSendTimeout( &sendTimeout ) )
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::sendto, sockDesc, (const void*)buffer, (size_t)bufferLen, 0, (const sockaddr *) &m_destAddr, (socklen_t)sizeof(m_destAddr)),
        sendTimeout );
#endif
}

//!Implementation of AbstractDatagramSocket::setDestAddr
bool UDPSocket::setDestAddr( const QString& foreignAddress, unsigned short foreignPort )
{
    return fillAddr( foreignAddress, foreignPort, m_destAddr );
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
    int rtn = recvfrom(sockDesc, (raw_type *) buffer, bufferLen, 0, (sockaddr *) &clntAddr, (socklen_t *) &addrLen);
#else
    unsigned int recvTimeout = 0;
    if( !getRecvTimeout( &recvTimeout ) )
        return -1;

    int rtn = doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::recvfrom, sockDesc, (void*)buffer, (size_t)bufferLen, 0, (sockaddr*)&clntAddr, (socklen_t*)&addrLen),
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
    FD_SET(sockDesc, &read_set);
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
    sockPollfd.fd = sockDesc;
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    return (::poll( &sockPollfd, 1, 0 ) == 1) && ((sockPollfd.revents & POLLIN) != 0);
#endif
}
