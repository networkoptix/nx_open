#include "socket.h"
#include "systemerror.h"


#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#endif


#ifdef Q_OS_WIN
typedef int socklen_t;
typedef char raw_type;       // Type used for raw data on this platform
#else
#include <sys/select.h>
#include <sys/types.h>       // For data types
#include <sys/socket.h>      // For socket(), connect(), send(), and recv()
#include <netdb.h>           // For getaddrinfo()
#include <arpa/inet.h>       // For inet_addr()
#include <unistd.h>          // For close()
#include <netinet/in.h>      // For sockaddr_in
#include <netinet/tcp.h>      // For TCP_NODELAY
#include <fcntl.h>
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

// SocketException Code

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

        m_lastError = QString("Couldn't resolve %1: %2.").arg(address).arg(errorMessage);
        return false;
    }

    addr.sin_addr.s_addr = ((struct sockaddr_in *) (addressInfo->ai_addr))->sin_addr.s_addr;
    addr.sin_port = htons(port);     // Assign port in network byte order

    freeaddrinfo(addressInfo);

    return true;
}

// Socket Code

Socket::Socket(int type, int protocol)
:
    m_nonBlockingMode( false )
{
    createSocket(type, protocol);
}

void Socket::createSocket(int type, int protocol)
{
#ifdef WIN32
    if (!initialized) {
        WORD wVersionRequested;
        WSADATA wsaData;

        wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
        if (WSAStartup(wVersionRequested, &wsaData) != 0) {  // Load WinSock DLL
            throw SocketException("Unable to load WinSock DLL.");
        }
        initialized = true;
    }
#endif

    // Make a new socket
    if ((sockDesc = socket(PF_INET, type, protocol)) < 0) {
        throw SocketException("Socket creation failed (socket()).", true);
    }
}

Socket::Socket(int sockDesc)
:
    m_nonBlockingMode( false )
{
    this->sockDesc = sockDesc;
}

Socket::~Socket() {
    close();
}

QString Socket::lastError() const
{
    return m_lastError;
}

void Socket::close()
{
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


QString Socket::getLocalAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return QString();
    }

    return QLatin1String(inet_ntoa(addr.sin_addr));
}

QString Socket::getPeerAddress() const
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

    if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0)
    {
        //error
        return false;
    }
    return true;
}

void Socket::cleanUp()  {
#ifdef WIN32
    if (WSACleanup() != 0) {
        throw SocketException("WSACleanup() failed.");
    }
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

// CommunicatingSocket Code

#ifndef _WIN32
namespace
{
    static const size_t MILLIS_IN_SEC = 1000;
    static const size_t NSECS_IN_MS = 1000000;

    template<class Func>
    int doInterruptableSystemCallWithTimeout( const Func& func, unsigned int timeout )
    {
        struct timespec waitStartTime;
        memset( &waitStartTime, 0, sizeof(waitStartTime) );
        bool waitStartTimeActual = false;
        if( timeout > 0 )
            waitStartTimeActual = clock_gettime( CLOCK_MONOTONIC, &waitStartTime ) == 0;
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
                struct timespec waitStopTime;
                memset( &waitStopTime, 0, sizeof(waitStopTime) );
                if( clock_gettime( CLOCK_MONOTONIC, &waitStopTime ) != 0 )
                    continue;   //not updating timeout value
                const int millisAlreadySlept = 
                    ((uint64_t)waitStopTime.tv_sec*MILLIS_IN_SEC + waitStopTime.tv_nsec/NSECS_IN_MS) - 
                    ((uint64_t)waitStartTime.tv_sec*MILLIS_IN_SEC + waitStartTime.tv_nsec/NSECS_IN_MS);
                if( (unsigned int)millisAlreadySlept < timeout )
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
      m_readTimeoutMS( 0 ),
      m_writeTimeoutMS( 0 ),
      mConnected(false)
{
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) 
    : Socket(newConnSD),
      m_readTimeoutMS( 0 ),
      m_writeTimeoutMS( 0 )
{
}

void CommunicatingSocket::close()
{
    Socket::close();
    mConnected = false;
}

bool CommunicatingSocket::connect(
    const QString &foreignAddress,
    unsigned short foreignPort,
    int timeoutMs )
{
    //TODO/IMPL non blocking connect

    m_lastError.clear();

    // Get the address of the requested host
    mConnected = false;

    sockaddr_in destAddr;
    if (!fillAddr(foreignAddress, foreignPort, destAddr))
        return false;

    //switching to non-blocking mode to connect with timeout
    const bool isNonBlockingModeBak = isNonBlockingMode();
    if( !isNonBlockingModeBak && !setNonBlockingMode( true ) )
        return false;

    int connectResult = ::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

    if( connectResult != 0 )
    {
        if( SystemError::getLastOSErrorCode() != SystemError::inProgress )
        {
            m_lastError = QString("Connect failed (connect()). %1").arg(SystemError::getLastOSErrorText());
            return false;
        }
        if( isNonBlockingModeBak )
            return true;        //async connect started
    }

    timeval timeVal;
    fd_set wrtFDS;
    int iSelRet = 0;

    /* monitor for incomming connections */
    FD_ZERO(&wrtFDS);
    FD_SET(sockDesc, &wrtFDS);

#ifdef _WIN32
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
    struct timespec waitStartTime;
    memset( &waitStartTime, 0, sizeof(waitStartTime) );
    bool waitStartTimeActual = false;
    if( timeoutMs >= 0 )
        waitStartTimeActual = clock_gettime( CLOCK_MONOTONIC, &waitStartTime ) == 0;
    for( ;; )
    {
        timeVal.tv_sec  = timeoutMs/1000;
        timeVal.tv_usec = timeoutMs%1000;
        iSelRet = ::select( sockDesc + 1, NULL, &wrtFDS, NULL, timeoutMs >= 0 ? &timeVal : NULL );
        if( iSelRet == -1 && errno == EINTR )
        {
            //modifying timeout for time we've already spent in select
            if( timeoutMs < 0 ||  //no timeout
                !waitStartTimeActual )
            {
                //not updating timeout value. This can lead to spending "tcp connect timeout" in select (if signals arrive frequently and no monotonic clock on system)
                continue;
            }
            struct timespec waitStopTime;
            memset( &waitStopTime, 0, sizeof(waitStopTime) );
            if( clock_gettime( CLOCK_MONOTONIC, &waitStopTime ) != 0 )
                continue;   //not updating timeout value
            const int millisAlreadySlept = 
                ((uint64_t)waitStopTime.tv_sec*MILLIS_IN_SEC + waitStopTime.tv_nsec/NSECS_IN_MS) - 
                ((uint64_t)waitStartTime.tv_sec*MILLIS_IN_SEC + waitStartTime.tv_nsec/NSECS_IN_MS);
            if( millisAlreadySlept >= timeoutMs )
                break;
            timeoutMs -= millisAlreadySlept;
            continue;
        }
        break;
    }
#endif

    mConnected = iSelRet > 0;
    //restoring original mode
    setNonBlockingMode( isNonBlockingModeBak );
    return mConnected;
}

void CommunicatingSocket::shutdown()
{
#ifdef Q_OS_WIN
    ::shutdown(sockDesc, SD_BOTH);
#else
    ::shutdown(sockDesc, SHUT_RDWR);
#endif
}

bool CommunicatingSocket::setReadTimeOut( unsigned int ms )
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
        //qnWarning("Timeout function failed.");
        return false;
    }
    m_readTimeoutMS = ms;
    return true;
}

bool CommunicatingSocket::setWriteTimeOut( unsigned int ms )
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
        //qnWarning("Timeout function failed.");
        return false;
    }
    m_writeTimeoutMS = ms;
    return true;
}

int CommunicatingSocket::send(const void *buffer, int bufferLen)
{
#ifdef _WIN32
    int sended = ::send(sockDesc, (raw_type *) buffer, bufferLen, 0);
    if (sended < 0) {
        int errCode = getSystemErrCode();
        if (/*errCode != ERR_TIMEOUT &&*/ errCode != ERR_WOULDBLOCK)    //TODO why not checking ERR_TIMEOUT? some kind of a hack?
            mConnected = false;
    }
    return sended;
#else
    int sended = doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::send, sockDesc, (const void*)buffer, (size_t)bufferLen, 0),
        m_writeTimeoutMS );
    if( sended == -1 && errno != ERR_TIMEOUT && errno != ERR_WOULDBLOCK )    //TODO why not checking ERR_TIMEOUT? some kind of a hack?
        mConnected = false;
    return sended;
#endif
}

int CommunicatingSocket::recv(void *buffer, int bufferLen, int flags)
{
#ifdef _WIN32
    int rtn = ::recv(sockDesc, (raw_type *) buffer, bufferLen, flags);
    if (rtn < 0)
    {
        int errCode = getSystemErrCode();
        if (errCode != ERR_TIMEOUT && errCode != ERR_WOULDBLOCK)
            mConnected = false;
    }
    return rtn;
#else
    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        stdext::bind<>(&::recv, sockDesc, (void*)buffer, (size_t)bufferLen, flags),
        m_readTimeoutMS );
    if( bytesRead == -1 && errno != ERR_TIMEOUT && errno != ERR_WOULDBLOCK )
        mConnected = false;
    return bytesRead;
#endif
}

QString CommunicatingSocket::getForeignAddress()
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
        //qnWarning("Fetch of foreign address failed (getpeername()).");
        return QString();
    }
    return QLatin1String(inet_ntoa(addr.sin_addr));
}

unsigned short CommunicatingSocket::getForeignPort()  {
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        //qnWarning("Fetch of foreign port failed (getpeername()).");
        return -1;
    }
    return ntohs(addr.sin_port);
}

// TCPSocket Code

TCPSocket::TCPSocket()
    : CommunicatingSocket(SOCK_STREAM,
                          IPPROTO_TCP) {
}

TCPSocket::TCPSocket(const QString &foreignAddress, unsigned short foreignPort)
    : CommunicatingSocket(SOCK_STREAM, IPPROTO_TCP) {
    connect(foreignAddress, foreignPort);
}

bool TCPSocket::reopen()
{
    close();
    try {
        createSocket(SOCK_STREAM, IPPROTO_TCP);
        return true;
    }
    catch(...) {
        return false;
    }
}

int TCPSocket::setNoDelay(bool value)
{
    int flag = value ? 1 : 0;
    return setsockopt(sockDesc,            // socket affected
                      IPPROTO_TCP,     // set option at TCP level
                      TCP_NODELAY,     // name of option
                      (char *) &flag,  // the cast is historical cruft
                      sizeof(int));    // length of option value
}

TCPSocket::TCPSocket(int newConnSD) : CommunicatingSocket(newConnSD) {
}

// TCPServerSocket Code

TCPServerSocket::TCPServerSocket(unsigned short localPort, int queueLen)
    : Socket(SOCK_STREAM, IPPROTO_TCP) {
    setLocalPort(localPort);
    setListen(queueLen);
}

TCPServerSocket::TCPServerSocket(const QString &localAddress,
                                 unsigned short localPort, int queueLen, bool reuseAddr)
    : Socket(SOCK_STREAM, IPPROTO_TCP) {
    setReuseAddrFlag(reuseAddr);
    if (!setLocalAddressAndPort(localAddress, localPort))
    {
        //qnWarning("Can't create socket: %1.", m_lastError);
        return;
    }

    setListen(queueLen);
}

TCPSocket *TCPServerSocket::accept()  {
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(sockDesc, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 250 * 1000;

    if (::select(sockDesc + 1, &read_set, NULL, NULL, &timeout) <= 0)
        return 0;

    int newConnSD;
    if ((newConnSD = ::accept(sockDesc, NULL, 0)) < 0)
    {
        return 0;
    }

    TCPSocket* result = new TCPSocket(newConnSD);
    result->mConnected = true;
    return result;
}

void TCPServerSocket::setListen(int queueLen)  {
    if (listen(sockDesc, queueLen) < 0) {
        throw SocketException(QString("Set listening socket failed (listen())."), true);
    }
}

//!if, \a val is \a true, turns non-blocking mode on, else turns it off
bool Socket::setNonBlockingMode(bool val)
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

//!Returns true, if in non-blocking mode
bool Socket::isNonBlockingMode() const
{
    return m_nonBlockingMode;
}

bool Socket::setReuseAddrFlag(bool reuseAddr)
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
        //qnWarning("Can't set SO_REUSEADDR flag to socket: %1.", strerror(errno));
        return false;
    }
    return true;
}

bool Socket::setLocalAddressAndPort(const QString &localAddress,
                                    unsigned short localPort)  
{
    m_lastError.clear();

    // Get the address of the requested host
    sockaddr_in localAddr;
    if (!fillAddr(localAddress, localPort, localAddr))
        return false;

    if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0) {
        SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        m_lastError = QString("Set of local address and port failed (bind()). %1").arg(SystemError::toString(errorCode));
        return false;
    }

    return true;
}
