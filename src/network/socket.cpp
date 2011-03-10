#include "Socket.h"
using namespace std;
#ifdef WIN32
  #include <winsock.h>         // For socket(), connect(), send(), and recv()
  typedef int socklen_t;
  typedef char raw_type;       // Type used for raw data on this platform
#else
  #include <sys/types.h>       // For data types
  #include <sys/socket.h>      // For socket(), connect(), send(), and recv()
  #include <netdb.h>           // For gethostbyname()
  #include <arpa/inet.h>       // For inet_addr()
  #include <unistd.h>          // For close()
  #include <netinet/in.h>      // For sockaddr_in
  typedef void raw_type;       // Type used for raw data on this platform
#endif

#include <errno.h>             // For errno

using namespace std;

#ifdef WIN32
static bool initialized = false;
#endif

// SocketException Code

SocketException::SocketException(const string &message, bool inclSysMsg)
  throw() : userMessage(message) {
  if (inclSysMsg) {
    userMessage.append(": ");
    userMessage.append(strerror(errno));
  }
}

SocketException::~SocketException() throw() {
}

const char *SocketException::what() const throw() {
  return userMessage.c_str();
}

// Function to fill in address structure given an address and port
static void fillAddr(const string &address, unsigned short port, 
                     sockaddr_in &addr) {
  memset(&addr, 0, sizeof(addr));  // Zero out address structure
  addr.sin_family = AF_INET;       // Internet address

  hostent *host;  // Resolve name
  if ((host = gethostbyname(address.c_str())) == NULL) {
    // strerror() will not work for gethostbyname() and hstrerror() 
    // is supposedly obsolete
    throw SocketException("Failed to resolve name (gethostbyname())");
  }
  addr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);

  addr.sin_port = htons(port);     // Assign port in network byte order
}

// Socket Code

Socket::Socket(int type, int protocol)  {
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
  #ifdef WIN32
    ::closesocket(sockDesc);
  #else
    ::close(sockDesc);
  #endif
  sockDesc = -1;
}

string Socket::getLocalAddress()  {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);

  if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
    throw SocketException("Fetch of local address failed (getsockname())", true);
  }
  return inet_ntoa(addr.sin_addr);
}

unsigned short Socket::getLocalPort()  {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);

  if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
    throw SocketException("Fetch of local port failed (getsockname())", true);
  }
  return ntohs(addr.sin_port);
}

void Socket::setLocalPort(unsigned short localPort)  {
  // Bind the socket to its port
  sockaddr_in localAddr;
  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  localAddr.sin_port = htons(localPort);

  if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0) {
    throw SocketException("Set of local port failed (bind())", true);
  }
}

void Socket::setLocalAddressAndPort(const string &localAddress,
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

unsigned short Socket::resolveService(const string &service,
                                      const string &protocol) {
  struct servent *serv;        /* Structure containing service information */

  if ((serv = getservbyname(service.c_str(), protocol.c_str())) == NULL)
    return atoi(service.c_str());  /* Service is port number */
  else 
    return ntohs(serv->s_port);    /* Found port (network byte order) by name */
}

// CommunicatingSocket Code

CommunicatingSocket::CommunicatingSocket(int type, int protocol)  
     : Socket(type, protocol),
m_timeout(3000)
{
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) : Socket(newConnSD),
m_timeout(3000)
{
}

void CommunicatingSocket::connect(const string &foreignAddress,
    unsigned short foreignPort)  {
  // Get the address of the requested host
  sockaddr_in destAddr;
  fillAddr(foreignAddress, foreignPort, destAddr);

  u_long iMode = 1;
  ioctlsocket(sockDesc, FIONBIO, &iMode); // set sock in asynch mode

  ::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr));// Try to connect to the given port

  TIMEVAL timeVal;
  FD_SET wrtFDS;
  int iSockRet, iSelRet;

  /* monitor for incomming connections */
  FD_ZERO(&wrtFDS);
  FD_SET(sockDesc, &wrtFDS);

  /* set timeout values */
  timeVal.tv_sec  = m_timeout/1000;
  timeVal.tv_usec = m_timeout%1000;

  iSelRet = ::select(0, NULL, &wrtFDS, NULL, &timeVal);

  if (iSelRet<=0)
	throw SocketException("Connect failed (connect())", true);

  iMode = 0;
  int t = ioctlsocket(sockDesc, FIONBIO, &iMode);  //set socket to synch  mode

}

void CommunicatingSocket::setTimeOut( unsigned int ms )
{
	m_timeout = ms;

	timeval tv;

	tv.tv_sec = ms/1000;
	tv.tv_usec = ms%1000;   //1 Secs Timeout 
	::setsockopt(sockDesc, SOL_SOCKET, SO_RCVTIMEO,(const char *)&tv,sizeof(struct timeval));

}

void CommunicatingSocket::send(const void *buffer, int bufferLen) 
     {
  if (::send(sockDesc, (raw_type *) buffer, bufferLen, 0) < 0) {
    throw SocketException("Send failed (send())", true);
  }
}

int CommunicatingSocket::recv(void *buffer, int bufferLen) 
{
	int rtn;
  if ((rtn = ::recv(sockDesc, (raw_type *) buffer, bufferLen, 0)) < 0) 
  {

	  int err = WSAGetLastError();
	  return -1;
  }

  return rtn;
}

string CommunicatingSocket::getForeignAddress() 
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

TCPSocket::TCPSocket(const std::string &foreignAddress, unsigned short foreignPort)
     : CommunicatingSocket(SOCK_STREAM, IPPROTO_TCP) {
  connect(foreignAddress, foreignPort);
}

TCPSocket::TCPSocket(int newConnSD) : CommunicatingSocket(newConnSD) {
}

// TCPServerSocket Code

TCPServerSocket::TCPServerSocket(unsigned short localPort, int queueLen) 
     : Socket(SOCK_STREAM, IPPROTO_TCP) {
  setLocalPort(localPort);
  setListen(queueLen);
}

TCPServerSocket::TCPServerSocket(const std::string &localAddress, 
    unsigned short localPort, int queueLen) 
     : Socket(SOCK_STREAM, IPPROTO_TCP) {
  setLocalAddressAndPort(localAddress, localPort);
  setListen(queueLen);
}

TCPSocket *TCPServerSocket::accept()  {
  int newConnSD;
  if ((newConnSD = ::accept(sockDesc, NULL, 0)) < 0) {
    throw SocketException("Accept failed (accept())", true);
  }

  return new TCPSocket(newConnSD);
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
 }

UDPSocket::UDPSocket(unsigned short localPort)   : 
    CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP) {
  setLocalPort(localPort);
  setBroadcast();
  m_destAddr = new sockaddr_in();
}

UDPSocket::UDPSocket(const std::string &localAddress, unsigned short localPort) 
      : CommunicatingSocket(SOCK_DGRAM, IPPROTO_UDP) {
  setLocalAddressAndPort(localAddress, localPort);
  setBroadcast();
  m_destAddr = new sockaddr_in();
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

void UDPSocket::setDestAddr(const std::string &foreignAddress, unsigned short foreignPort)
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

int UDPSocket::recvFrom(void *buffer, int bufferLen, string &sourceAddress,
    unsigned short &sourcePort)  {
  sockaddr_in clntAddr;
  socklen_t addrLen = sizeof(clntAddr);
  int rtn;
  if ((rtn = recvfrom(sockDesc, (raw_type *) buffer, bufferLen, 0, 
                      (sockaddr *) &clntAddr, (socklen_t *) &addrLen)) < 0) {
    throw SocketException("Receive failed (recvfrom())", true);
  }
  sourceAddress = inet_ntoa(clntAddr.sin_addr);
  sourcePort = ntohs(clntAddr.sin_port);

  return rtn;
}

void UDPSocket::setMulticastTTL(unsigned char multicastTTL)  {
  if (setsockopt(sockDesc, IPPROTO_IP, IP_MULTICAST_TTL, 
                 (raw_type *) &multicastTTL, sizeof(multicastTTL)) < 0) {
    throw SocketException("Multicast TTL set failed (setsockopt())", true);
  }
}

void UDPSocket::joinGroup(const string &multicastGroup)  {
  struct ip_mreq multicastRequest;

  multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.c_str());
  multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                 (raw_type *) &multicastRequest, 
                 sizeof(multicastRequest)) < 0) {
    throw SocketException("Multicast group join failed (setsockopt())", true);
  }
}

void UDPSocket::leaveGroup(const string &multicastGroup)  {
  struct ip_mreq multicastRequest;

  multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.c_str());
  multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
                 (raw_type *) &multicastRequest, 
                 sizeof(multicastRequest)) < 0) {
    throw SocketException("Multicast group leave failed (setsockopt())", true);
  }
}
