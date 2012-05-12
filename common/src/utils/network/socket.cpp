#include "socket.h"
#include "../common/log.h"

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
  #include <netdb.h>           // For gethostbyname()
  #include <arpa/inet.h>       // For inet_addr()
  #include <unistd.h>          // For close()
  #include <netinet/in.h>      // For sockaddr_in
  #include <netinet/tcp.h>      // For TCP_NODELAY
  #include <fcntl.h>
#include "../common/log.h"
  typedef void raw_type;       // Type used for raw data on this platform
#endif

#ifdef WIN32
static bool initialized = false;
static const int ERR_TIMEOUT = WSAETIMEDOUT;
#else
static const int ERR_TIMEOUT = EAGAIN;
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
    userMessage.append(": ");
    userMessage.append(strerror(errno));
  }

  QByteArray data = userMessage.toAscii();
  strncpy(m_message, data.data(), MAX_ERROR_MSG_LENGTH-1);
  m_message[MAX_ERROR_MSG_LENGTH-1] = 0;
}

SocketException::~SocketException() throw() {
}

const char *SocketException::what() const throw() {
  return m_message;
}

// Function to fill in address structure given an address and port
static void fillAddr(const QString &address, unsigned short port,
                     sockaddr_in &addr) {
  memset(&addr, 0, sizeof(addr));  // Zero out address structure
  addr.sin_family = AF_INET;       // Internet address

  hostent *host;  // Resolve name
  if ((host = gethostbyname(address.toAscii())) == NULL) {
    // strerror() will not work for gethostbyname() and hstrerror()
    // is supposedly obsolete
    throw SocketException("Failed to resolve name (gethostbyname())");
  }
  addr.sin_addr.s_addr = *((quint32 *) host->h_addr_list[0]);

  addr.sin_port = htons(port);     // Assign port in network byte order
}

// Socket Code

Socket::Socket(int type, int protocol)  {
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
            throw SocketException("Unable to load WinSock DLL");
        }
        initialized = true;
    }
#endif

    // Make a new socket
    if ((sockDesc = socket(PF_INET, type, protocol)) < 0) {
        throw SocketException("Socket creation failed (socket())", true);
    }
}

Socket::Socket(int sockDesc) {
  this->sockDesc = sockDesc;
}

Socket::~Socket() {
    close();
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
    return "";
  }

  return inet_ntoa(addr.sin_addr);
}

QString Socket::getPeerAddress() const
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)
    {
        return "";
    }

    return inet_ntoa(addr.sin_addr);
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

void Socket::setLocalAddressAndPort(const QString &localAddress,
    unsigned short localPort)  {
  // Get the address of the requested host
  sockaddr_in localAddr;
  fillAddr(localAddress, localPort, localAddr);

  if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0) {
    throw SocketException("Set of local address and port failed (bind())", true);
  }
}

void Socket::cleanUp()  {
  #ifdef WIN32
    if (WSACleanup() != 0) {
      throw SocketException("WSACleanup() failed");
    }
  #endif
}

unsigned short Socket::resolveService(const QString &service,
                                      const QString &protocol) {
  struct servent *serv;        /* Structure containing service information */

  if ((serv = getservbyname(service.toAscii(), protocol.toAscii())) == NULL)
    return atoi(service.toAscii());  /* Service is port number */
  else
    return ntohs(serv->s_port);    /* Found port (network byte order) by name */
}

// CommunicatingSocket Code

CommunicatingSocket::CommunicatingSocket(int type, int protocol)
     : Socket(type, protocol),
m_timeout(3000),
mConnected(false)
{
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) : Socket(newConnSD),
m_timeout(3000)
{
}

bool CommunicatingSocket::isConnected() const
{
    return mConnected;
}

void CommunicatingSocket::close()
{
    Socket::close();
    mConnected = false;
}


bool CommunicatingSocket::connect(const QString &foreignAddress,
    unsigned short foreignPort)
{
  // Get the address of the requested host
    mConnected = false;

  sockaddr_in destAddr;
  fillAddr(foreignAddress, foreignPort, destAddr);

  u_long iMode = 1;
#ifdef _WIN32
  ioctlsocket(sockDesc, FIONBIO, &iMode); // set sock in asynch mode
#else
  // fcntl(sockDesc, F_SETFL, O_NONBLOCK);
#endif

  int connectResult = ::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

#ifndef _WIN32
  if (connectResult != 0)
  {
      //throw SocketException("Connect failed (connect())", true);
      return false;
  }
#else
  Q_UNUSED(connectResult);
#endif

#ifdef _WIN32
  timeval timeVal;
  fd_set wrtFDS;
  int iSelRet;

  /* monitor for incomming connections */
  FD_ZERO(&wrtFDS);
  FD_SET(sockDesc, &wrtFDS);

  /* set timeout values */
  timeVal.tv_sec  = m_timeout/1000;
  timeVal.tv_usec = m_timeout%1000;

  iSelRet = ::select(sockDesc + 1, NULL, &wrtFDS, NULL, &timeVal);

  if (iSelRet<=0)
    return false;
#endif // _WIN32

  iMode = 0;
#ifdef _WIN32
    ioctlsocket(sockDesc, FIONBIO, &iMode); // set sock in asynch mode
#else
    // fcntl(sockDesc, F_SETFL, 0);
#endif

  mConnected = true;

  return true;
}

void CommunicatingSocket::shutdown()
{
#ifdef Q_OS_WIN
    ::shutdown(sockDesc, SD_BOTH);
#else
    ::shutdown(sockDesc, SHUT_RDWR);
#endif
}

void CommunicatingSocket::setReadTimeOut( unsigned int ms )
{
	m_timeout = ms;

	timeval tv;

	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
	if ( setsockopt (sockDesc, SOL_SOCKET, SO_RCVTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVTIMEO,(const void *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        cl_log.log("Timeout function failed", cl_logALWAYS);
    }
}

void CommunicatingSocket::setWriteTimeOut( unsigned int ms )
{
    m_timeout = ms;

    timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if ( setsockopt (sockDesc, SOL_SOCKET, SO_SNDTIMEO, ( char* )&ms,  sizeof ( ms ) ) != 0)
#else
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_SNDTIMEO,(const char *)&tv,sizeof(struct timeval)) < 0)
#endif
    {
        cl_log.log("Timeout function failed", cl_logALWAYS);
    }
}

int CommunicatingSocket::send(const void *buffer, int bufferLen)
{
    int sended = ::send(sockDesc, (raw_type *) buffer, bufferLen, 0);
    if (sended < 0) {
        //int errCode = getSystemErrCode();
        //if (errCode != ERR_TIMEOUT)
            mConnected = false;
    }
    return sended;
}

int CommunicatingSocket::recv(void *buffer, int bufferLen, int flags)
{
    int rtn = ::recv(sockDesc, (raw_type *) buffer, bufferLen, flags);
    if (rtn < 0)
    {
        int errCode = getSystemErrCode();
        if (errCode != ERR_TIMEOUT)
        mConnected = false;
            return -1;
    }
    return rtn;
}

QString CommunicatingSocket::getForeignAddress()
     {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);

  if (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0) {
    throw SocketException("Fetch of foreign address failed (getpeername())", true);
  }
  return inet_ntoa(addr.sin_addr);
}

unsigned short CommunicatingSocket::getForeignPort()  {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);

  if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
    throw SocketException("Fetch of foreign port failed (getpeername())", true);
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
  setLocalAddressAndPort(localAddress, localPort);
  setListen(queueLen);
}

TCPSocket *TCPServerSocket::accept()  {
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(sockDesc, &read_set);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

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
    throw SocketException("Set listening socket failed (listen())", true);
  }
}

// UDPSocket Code

UDPSocket::UDPSocket()  : CommunicatingSocket(SOCK_DGRAM,
    IPPROTO_UDP)
{
    setBroadcast();
    m_destAddr = new sockaddr_in();
    int buff_size = 1024*512;
    if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
    {
        //error
    }
 }

UDPSocket::UDPSocket(unsigned short localPort)   :
    CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP) {
  setLocalPort(localPort);
  setBroadcast();
  m_destAddr = new sockaddr_in();

  int buff_size = 1024*512;
  if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
  {
      //error
  }
}

UDPSocket::UDPSocket(const QString &localAddress, unsigned short localPort)
      : CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP) 
{
  setLocalAddressAndPort(localAddress, localPort);
  setBroadcast();
  m_destAddr = new sockaddr_in();
  int buff_size = 1024*512;
  if (::setsockopt(sockDesc, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size))<0)
  {
      //error
  }
}

UDPSocket::~UDPSocket()
{
    delete m_destAddr;
}

void UDPSocket::setBroadcast() {
  // If this fails, we'll hear about it when we try to send.  This will allow
  // system that cannot broadcast to continue if they don't plan to broadcast
  int broadcastPermission = 1;
  setsockopt(sockDesc, SOL_SOCKET, SO_BROADCAST,
             (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

void UDPSocket::disconnect()  {
  sockaddr_in nullAddr;
  memset(&nullAddr, 0, sizeof(nullAddr));
  nullAddr.sin_family = AF_UNSPEC;

  // Try to disconnect
  if (::connect(sockDesc, (sockaddr *) &nullAddr, sizeof(nullAddr)) < 0) {
   #ifdef WIN32
    if (errno != WSAEAFNOSUPPORT) {
   #else
    if (errno != EAFNOSUPPORT) {
   #endif
      throw SocketException("Disconnect failed (connect())", true);
    }
  }
}

void UDPSocket::setDestPort(unsigned short foreignPort)
{
    m_destAddr->sin_port = htons(foreignPort);
}

void UDPSocket::setDestAddr(const QString &foreignAddress, unsigned short foreignPort)
{
    fillAddr(foreignAddress, foreignPort, *m_destAddr);
}

bool UDPSocket::sendTo(const void *buffer, int bufferLen)
{

	// Write out the whole buffer as a single message.
	if (sendto(sockDesc, (raw_type *) buffer, bufferLen, 0,
	(sockaddr *) m_destAddr, sizeof(sockaddr_in)) != bufferLen)
	{
		return false;
	}

	return true;

}

int UDPSocket::recvFrom(void *buffer, int bufferLen, QString &sourceAddress,
    unsigned short &sourcePort)  
{
    sockaddr_in clntAddr;
    socklen_t addrLen = sizeof(clntAddr);
    int rtn = recvfrom(sockDesc, (raw_type *) buffer, bufferLen, 0, (sockaddr *) &clntAddr, (socklen_t *) &addrLen);
    if (rtn >= 0) {
        sourceAddress = inet_ntoa(clntAddr.sin_addr);
        sourcePort = ntohs(clntAddr.sin_port);
    }
    return rtn;
}

void UDPSocket::setMulticastTTL(unsigned char multicastTTL)  {
  if (setsockopt(sockDesc, IPPROTO_IP, IP_MULTICAST_TTL,
                 (raw_type *) &multicastTTL, sizeof(multicastTTL)) < 0) {
    throw SocketException("Multicast TTL set failed (setsockopt())", true);
  }
}

void UDPSocket::joinGroup(const QString &multicastGroup)  {
  struct ip_mreq multicastRequest;

  multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toAscii());
  multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                 (raw_type *) &multicastRequest,
                 sizeof(multicastRequest)) < 0) {
    throw SocketException("Multicast group join failed (setsockopt())", true);
  }
}

void UDPSocket::leaveGroup(const QString &multicastGroup)  {
  struct ip_mreq multicastRequest;

  multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toAscii());
  multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                 (raw_type *) &multicastRequest,
                 sizeof(multicastRequest)) < 0) {
    throw SocketException("Multicast group leave failed (setsockopt())", true);
  }
}

bool UDPSocket::hasData() const
{
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(sockDesc, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    switch( ::select(FD_SETSIZE, &read_set, NULL, NULL, &timeout))
    {
        case 0:				// timeout expired
            {
                return false;
            }
        case SOCKET_ERROR: 	// error occured
            {
                return false;
            }
    }
    return true;
}

void Socket::setReuseAddrFlag(bool reuseAddr)
{
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal))) {
        QString message = QString("Can't set SO_REUSEADDR flag to socket: %1").arg(::strerror(errno));
        throw SocketException(message, true);
    }
}

