#ifndef __PRACTICALSOCKET_INCLUDED__
#define __PRACTICALSOCKET_INCLUDED__

#include <string>
#include <exception>

#include <QtCore/QString>
#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include "abstract_socket.h"
#include "nettools.h"
#include "socket_factory.h"
#include "utils/common/byte_array.h"
#include "../common/systemerror.h"

// TODO: #Elric why bother with maxlen and not use QByteArray directly? Remove.
#define MAX_ERROR_MSG_LENGTH 1024

// TODO: #Elric inherit from QnException?
// 
/**
 *   Signals a problem with the execution of a socket call.
 */
class SocketException : public std::exception {
public:
    /**
      *   Construct a SocketException with a explanatory message.
      *   @param message explanatory message
      *   @param incSysMsg true if system message (from strerror(errno))
      *   should be postfixed to the user provided message
    */
    SocketException(const QString &message, bool inclSysMsg = false) throw();

    /**
     *   Provided just to guarantee that no exceptions are thrown.
     */
    ~SocketException() throw();

    /**
     *   Get the exception message
     *   @return exception message
     */
    const char *what() const throw();

private:
    char m_message[MAX_ERROR_MSG_LENGTH];
};


class SocketImpl;

/**
 *   Base class representing basic communication endpoint
 */
class Socket
:
    virtual public AbstractSocket
{
    Q_DECLARE_TR_FUNCTIONS(Socket)

public:
    //TODO/IMPL draft refactoring of socket operation result receiving. Enum introduced like in std stream bits
    enum StatusBit
    {
        sbFailed = 1
    };

    /**
     *   Close and deallocate this socket
     */
    virtual ~Socket();


    //!Implementation of AbstractSocket::bind
    virtual bool bind( const SocketAddress& localAddress ) override;
    //!Implementation of AbstractSocket::bindToInterface
    //virtual bool bindToInterface( const QnInterfaceAndAddr& iface ) override;
    //!Implementation of AbstractSocket::getLocalAddress
    virtual SocketAddress getLocalAddress() const override;
    //!Implementation of AbstractSocket::getPeerAddress
    virtual SocketAddress getPeerAddress() const override;
    //!Implementation of AbstractSocket::close
    virtual void close() override;
    //!Implementation of AbstractSocket::isClosed
    virtual bool isClosed() const override;
    //!Implementation of AbstractSocket::setReuseAddrFlag
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    //!Implementation of AbstractSocket::reuseAddrFlag
    virtual bool getReuseAddrFlag( bool* val ) override;
    //!Implementation of AbstractSocket::setNonBlockingMode
    virtual bool setNonBlockingMode( bool val ) override;
    //!Implementation of AbstractSocket::getNonBlockingMode
    virtual bool getNonBlockingMode( bool* val ) const override;
    //!Implementation of AbstractSocket::getMtu
    virtual bool getMtu( unsigned int* mtuValue ) override;
    //!Implementation of AbstractSocket::setSendBufferSize
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    //!Implementation of AbstractSocket::getSendBufferSize
    virtual bool getSendBufferSize( unsigned int* buffSize ) override;
    //!Implementation of AbstractSocket::setRecvBufferSize
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    //!Implementation of AbstractSocket::getRecvBufferSize
    virtual bool getRecvBufferSize( unsigned int* buffSize ) override;
    //!Implementation of AbstractSocket::setRecvTimeout
    virtual bool setRecvTimeout( unsigned int ms ) override;
    //!Implementation of AbstractSocket::getRecvTimeout
    virtual bool getRecvTimeout( unsigned int* millis ) override;
    //!Implementation of AbstractSocket::setSendTimeout
    virtual bool setSendTimeout( unsigned int ms ) override;
    //!Implementation of AbstractSocket::getSendTimeout
    virtual bool getSendTimeout( unsigned int* millis ) override;
    //!Implementation of AbstractSocket::handle
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;


    QString lastError() const;

    /**
     *   Get the local address
     *   @return local address of socket
     *   @exception SocketException thrown if fetch fails
     */
    QString getLocalHostAddress() const;

    /**
     *   Get the peer address
     *   @return remove address of socket
     *   @exception SocketException thrown if fetch fails
     */
    QString getPeerHostAddress() const;
    quint32 getPeerAddressUint() const;

    /**
     *   Get the local port
     *   @return local port of socket
     *   @exception SocketException thrown if fetch fails
     */
    unsigned short getLocalPort() const;

    /**
     *   Set the local port to the specified port and the local address
     *   to any interface
     *   @param localPort local port
     *   @exception SocketException thrown if setting local port fails
     */
    bool setLocalPort(unsigned short localPort) ;

    /**
     *   Set the local port to the specified port and the local address
     *   to the specified address.  If you omit the port, a random port
     *   will be selected.
     *   @param localAddress local address
     *   @param localPort local port
     *   @exception SocketException thrown if setting local port or address fails
     */
    bool setLocalAddressAndPort(const QString &localAddress,
                                unsigned short localPort = 0) ;

    //!Returns socket write/connect timeout in millis
    unsigned int getWriteTimeOut() const;

    /**
     *   If WinSock, unload the WinSock DLLs; otherwise do nothing.  We ignore
     *   this in our sample client code but include it in the library for
     *   completeness.  If you are running on Windows and you are concerned
     *   about DLL resource consumption, call this after you are done with all
     *   Socket instances.  If you execute this on Windows while some instance of
     *   Socket exists, you are toast.  For portability of client code, this is
     *   an empty function on non-Windows platforms so you can always include it.
     *   @param buffer buffer to receive the data
     *   @param bufferLen maximum number of bytes to read into buffer
     *   @return number of bytes read, 0 for EOF, and -1 for error
     *   @exception SocketException thrown WinSock clean up fails
     */
    static void cleanUp() ;

    /**
     *   Resolve the specified service for the specified protocol to the
     *   corresponding port number in host byte order
     *   @param service service to resolve (e.g., "http")
     *   @param protocol protocol of service to resolve.  Default is "tcp".
     */
    static unsigned short resolveService(const QString &service,
                                         const QString &protocol = QLatin1String("tcp"));

    bool failed() const;
    SystemError::ErrorCode prevErrorCode() const;

    SocketImpl* impl();
    const SocketImpl* impl() const;

protected:
    int sockDesc;              // Socket descriptor
    QString m_lastError;

    Socket(int type, int protocol) ;
    Socket(int sockDesc);
    bool fillAddr(const QString &address, unsigned short port, sockaddr_in &addr);
    bool createSocket(int type, int protocol);
    void setStatusBit( StatusBit _status );
    void clearStatusBit( StatusBit _status );
    void saveErrorInfo();

private:
    SocketImpl* m_impl;
    bool m_nonBlockingMode;
    unsigned int m_status;
    SystemError::ErrorCode m_prevErrorCode;
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;

    // Prevent the user from trying to use value semantics on this object
    Socket(const Socket &sock);
    void operator=(const Socket &sock);
};

/**
 *   Socket which is able to connect, send, and receive
 */
class CommunicatingSocket
:
    public Socket,
    virtual public AbstractCommunicatingSocket
{
    Q_DECLARE_TR_FUNCTIONS(CommunicatingSocket)

public:
    //!Implementation of AbstractCommunicatingSocket::connect
    virtual bool connect(
        const QString &foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis ) override;
    //!Implementation of AbstractCommunicatingSocket::recv
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override;
    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    //!Implementation of AbstractCommunicatingSocket::getForeignAddress
    virtual const SocketAddress getForeignAddress() override;
    //!Implementation of AbstractCommunicatingSocket::isConnected
    virtual bool isConnected() const override;


    void shutdown();
    virtual void close();


    /**
     *   Get the foreign address.  Call connect() before calling recv()
     *   @return foreign address
     *   @exception SocketException thrown if unable to fetch foreign address
     */
    QString getForeignHostAddress() ;

    /**
     *   Get the foreign port.  Call connect() before calling recv()
     *   @return foreign port
     *   @exception SocketException thrown if unable to fetch foreign port
     */
    unsigned short getForeignPort() ;

protected:
    CommunicatingSocket(int type, int protocol) ;
    CommunicatingSocket(int newConnSD);

protected:
    bool mConnected;
};

/**
 *   TCP socket for communication with other TCP sockets
 */
class TCPSocket
:
    public CommunicatingSocket,
    virtual public AbstractStreamSocket
{
    Q_DECLARE_TR_FUNCTIONS(TCPSocket)

public:
    /**
     *   Construct a TCP socket with no connection
     *   @exception SocketException thrown if unable to create TCP socket
     */
    TCPSocket() ;

    /**
     *   Construct a TCP socket with a connection to the given foreign address
     *   and port
     *   @param foreignAddress foreign address (IP address or name)
     *   @param foreignPort foreign port
     *   @exception SocketException thrown if unable to create TCP socket
     */
    TCPSocket(const QString &foreignAddress, unsigned short foreignPort);

    //!Implementation of AbstractStreamSocket::reopen
    virtual bool reopen() override;
    //!Implementation of AbstractStreamSocket::setNoDelay
    virtual bool setNoDelay( bool value ) override;
    //!Implementation of AbstractStreamSocket::getNoDelay
    virtual bool getNoDelay( bool* value ) override;

private:
    // Access for TCPServerSocket::accept() connection creation
    friend class TCPServerSocket;
    TCPSocket(int newConnSD);
};

/**
 *   TCP socket class for servers
 */
class TCPServerSocket
:
    public Socket,
    virtual public AbstractStreamServerSocket
{
    Q_DECLARE_TR_FUNCTIONS(TCPServerSocket)

public:
    TCPServerSocket();

    /**
     *   Blocks until a new connection is established on this socket or error
     *   @return new connection socket
     *   @exception SocketException thrown if attempt to accept a new connection fails
     */
    static int accept(int sockDesc);


    //!Implementation of AbstractStreamServerSocket::listen
    virtual bool listen( int queueLen ) override;
    //!Implementation of AbstractStreamServerSocket::accept
    virtual AbstractStreamSocket* accept() override;

private:
    bool setListen(int queueLen) ;
};

#ifdef ENABLE_SSL
class TCPSslServerSocket: public TCPServerSocket
{
public:
    /*
    *   allowNonSecureConnect - allow mixed ssl and non ssl connect for socket
    */
    TCPSslServerSocket(bool allowNonSecureConnect = true);

    virtual AbstractStreamSocket* accept() override;
private:
    bool m_allowNonSecureConnect;
};
#endif // ENABLE_SSL

/**
  *   UDP socket class
  */
class UDPSocket
:
    public CommunicatingSocket,
    virtual public AbstractDatagramSocket
{
    Q_DECLARE_TR_FUNCTIONS(UDPSocket)

public:
    static const unsigned int MAX_PACKET_SIZE = 64*1024 - 24 - 8;   //maximum ip datagram size - ip header length - udp header length

    /**
     *   Construct a UDP socket
     *   @exception SocketException thrown if unable to create UDP socket
     */
    UDPSocket() ;

    /**
     *   Construct a UDP socket with the given local port
     *   @param localPort local port
     *   @exception SocketException thrown if unable to create UDP socket
     */
    UDPSocket(unsigned short localPort) ;

    /**
     *   Construct a UDP socket with the given local port and address
     *   @param localAddress local address
     *   @param localPort local port
     *   @exception SocketException thrown if unable to create UDP socket
     */
    UDPSocket(const QString &localAddress, unsigned short localPort);

    void setDestPort(unsigned short foreignPort);

    /**
     *   Unset foreign address and port
     *   @return true if disassociation is successful
     *   @exception SocketException thrown if unable to disconnect UDP socket
     */
    //void disconnect() ;

    /**
     *   Send the given buffer as a UDP datagram to the
     *   specified address/port
     *   @param buffer buffer to be written
     *   @param bufferLen number of bytes to write
     *   @param foreignAddress address (IP address or name) to send to
     *   @param foreignPort port number to send to
     *   @return true if send is successful
     *
     */
    bool sendTo(const void *buffer, int bufferLen);

    /**
     *   Set the multicast TTL
     *   @param multicastTTL multicast TTL
     */
    bool setMulticastTTL(unsigned char multicastTTL) ;

    /**
     *   Set the multicast send interface
     *   @param multicastIF multicast interface for sending packets
     */
    virtual bool setMulticastIF(const QString& multicastIF) override;

    /**
     *   Join the specified multicast group
     *   @param multicastGroup multicast group address to join
     */
    bool joinGroup(const QString &multicastGroup);
    bool joinGroup(const QString &multicastGroup, const QString& multicastIF);

    /**
     *   Leave the specified multicast group
     *   @param multicastGroup multicast group address to leave
     */
    bool leaveGroup(const QString &multicastGroup);
    bool leaveGroup(const QString &multicastGroup, const QString& multicastIF);

    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    //!Implementation of AbstractDatagramSocket::setDestAddr
    virtual bool setDestAddr( const QString& foreignAddress, unsigned short foreignPort ) override;
    //!Implementation of AbstractDatagramSocket::sendTo
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const QString& foreignAddress,
        unsigned short foreignPort ) override;
    //!Implementation of AbstractDatagramSocket::recvFrom
    virtual int recvFrom(
        void* buffer,
        int bufferLen,
        QString& sourceAddress,
        unsigned short& sourcePort ) override;
    //!Implementation of AbstractDatagramSocket::hasData
    virtual bool hasData() const override;

private:
    void setBroadcast();

private:
    sockaddr_in m_destAddr;
};

#endif
