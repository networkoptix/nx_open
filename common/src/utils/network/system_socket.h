#ifndef __PRACTICALSOCKET_INCLUDED__
#define __PRACTICALSOCKET_INCLUDED__

#include <string>
#include <exception>

#include <QtCore/QString>

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
#include "socket_impl_helper.h"
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
{
public:
    Socket( int type, int protocol, SocketImpl* impl = nullptr );
    Socket( int sockDesc, SocketImpl* impl = nullptr );

    /**
     *   Close and deallocate this socket
     */
    virtual ~Socket();


    //!Implementation of AbstractSocket::bind
    bool bind( const SocketAddress& localAddress );
    //!Implementation of AbstractSocket::bindToInterface
    //bool bindToInterface( const QnInterfaceAndAddr& iface );
    //!Implementation of AbstractSocket::getLocalAddress
    SocketAddress getLocalAddress() const;
    //!Implementation of AbstractSocket::getPeerAddress
    SocketAddress getPeerAddress() const;
    //!Implementation of AbstractSocket::close
    virtual void close();
    //!Implementation of AbstractSocket::isClosed
    bool isClosed() const;
    //!Implementation of AbstractSocket::setReuseAddrFlag
    bool setReuseAddrFlag( bool reuseAddr );
    //!Implementation of AbstractSocket::reuseAddrFlag
    bool getReuseAddrFlag( bool* val ) const;
    //!Implementation of AbstractSocket::setNonBlockingMode
    bool setNonBlockingMode( bool val );
    //!Implementation of AbstractSocket::getNonBlockingMode
    bool getNonBlockingMode( bool* val ) const;
    //!Implementation of AbstractSocket::getMtu
    bool getMtu( unsigned int* mtuValue ) const;
    //!Implementation of AbstractSocket::setSendBufferSize
    bool setSendBufferSize( unsigned int buffSize );
    //!Implementation of AbstractSocket::getSendBufferSize
    bool getSendBufferSize( unsigned int* buffSize ) const;
    //!Implementation of AbstractSocket::setRecvBufferSize
    bool setRecvBufferSize( unsigned int buffSize );
    //!Implementation of AbstractSocket::getRecvBufferSize
    bool getRecvBufferSize( unsigned int* buffSize ) const;
    //!Implementation of AbstractSocket::setRecvTimeout
    bool setRecvTimeout( unsigned int ms );
    //!Implementation of AbstractSocket::getRecvTimeout
    bool getRecvTimeout( unsigned int* millis ) const;
    //!Implementation of AbstractSocket::setSendTimeout
    bool setSendTimeout( unsigned int ms );
    //!Implementation of AbstractSocket::getSendTimeout
    bool getSendTimeout( unsigned int* millis ) const;
    //!Implementation of AbstractSocket::getLastError
    bool getLastError( SystemError::ErrorCode* errorCode ) const;
    //!Implementation of AbstractSocket::handle
    AbstractSocket::SOCKET_HANDLE handle() const;


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

    SocketImpl* impl();
    const SocketImpl* impl() const;

    bool fillAddr( const QString &address, unsigned short port, sockaddr_in &addr );
    bool createSocket( int type, int protocol );

protected:
    int m_socketHandle;              // Socket descriptor
    SocketImpl* m_impl;

private:
    bool m_nonBlockingMode;
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;

    // Prevent the user from trying to use value semantics on this object
    Socket(const Socket &sock);
    void operator=(const Socket &sock);
};

template<class SocketType> class AsyncSocketImplHelper;

/**
 *   Socket which is able to connect, send, and receive
 */
class CommunicatingSocket
:
    public Socket
{
public:
    CommunicatingSocket( AbstractCommunicatingSocket* abstractSocketPtr, int type, int protocol, SocketImpl* sockImpl = nullptr );
    CommunicatingSocket( AbstractCommunicatingSocket* abstractSocketPtr, int newConnSD, SocketImpl* sockImpl = nullptr );

    virtual ~CommunicatingSocket();

    //!Implementation of AbstractCommunicatingSocket::connect
    bool connect(
        const QString &foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis );
    //!Implementation of AbstractCommunicatingSocket::recv
    int recv( void* buffer, unsigned int bufferLen, int flags );
    //!Implementation of AbstractCommunicatingSocket::send
    int send( const void* buffer, unsigned int bufferLen );
    //!Implementation of AbstractCommunicatingSocket::getForeignAddress
    virtual SocketAddress getForeignAddress() const;
    //!Implementation of AbstractCommunicatingSocket::isConnected
    bool isConnected() const;
    //!Implementation of AbstractCommunicatingSocket::connectAsyncImpl
    bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler );
    //!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
    bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler );
    //!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
    bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler );
    //!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
    void cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion );


    void shutdown();
    virtual void close() override;


    /**
     *   Get the foreign address.  Call connect() before calling recv()
     *   @return foreign address
     *   @exception SocketException thrown if unable to fetch foreign address
     */
    QString getForeignHostAddress() const;

    /**
     *   Get the foreign port.  Call connect() before calling recv()
     *   @return foreign port
     *   @exception SocketException thrown if unable to fetch foreign port
     */
    unsigned short getForeignPort() const;

private:
    std::unique_ptr<AsyncSocketImplHelper<Socket>> m_aioHelper;
    bool m_connected;
};


/**
 *   TCP socket for communication with other TCP sockets
 */
class TCPSocket
:
    public AbstractCommunicatingSocketImplementationDelegate<AbstractStreamSocket, CommunicatingSocket>
{
    typedef AbstractCommunicatingSocketImplementationDelegate<AbstractStreamSocket, CommunicatingSocket> base_type;

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
    //!User by \a TCPServerSocket class
    TCPSocket( int newConnSD );
    virtual ~TCPSocket();


    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractStreamSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractStreamSocket::reopen
    virtual bool reopen() override;
    //!Implementation of AbstractStreamSocket::setNoDelay
    virtual bool setNoDelay( bool value ) override;
    //!Implementation of AbstractStreamSocket::getNoDelay
    virtual bool getNoDelay( bool* value ) const override;
    //!Implementation of AbstractStreamSocket::toggleStatisticsCollection
    virtual bool toggleStatisticsCollection( bool val ) override;
    //!Implementation of AbstractStreamSocket::getConnectionStatistics
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;

private:
    // Access for TCPServerSocket::accept() connection creation
    friend class TCPServerSocket;
};

/**
 *   TCP socket class for servers
 */
class TCPServerSocket
:
    public AbstractSocketImplementationEmbeddingDelegate<AbstractStreamServerSocket, Socket>
{
    typedef AbstractSocketImplementationEmbeddingDelegate<AbstractStreamServerSocket, Socket> base_type;

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
    //!Implementation of AbstractStreamServerSocket::cancelAsyncIO
    virtual void cancelAsyncIO( bool waitForRunningHandlerCompletion ) override;

protected:
    //!Implementation of AbstractStreamServerSocket::acceptAsyncImpl
    virtual bool acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>&& handler ) override;

private:
    bool setListen(int queueLen) ;
};

/**
  *   UDP socket class
  */
class UDPSocket
:
    public AbstractCommunicatingSocketImplementationDelegate<AbstractDatagramSocket, CommunicatingSocket>
{
    typedef AbstractCommunicatingSocketImplementationDelegate<AbstractDatagramSocket, CommunicatingSocket> base_type;

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

    //!Implementation of AbstractDatagramSocket::joinGroup
    virtual bool joinGroup( const QString &multicastGroup ) override;
    virtual bool joinGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    //!Implementation of AbstractDatagramSocket::leaveGroup
    virtual bool leaveGroup( const QString &multicastGroup ) override;
    virtual bool leaveGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    //!Implementation of AbstractCommunicatingSocket::send
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    //!Implementation of AbstractDatagramSocket::setDestAddr
    virtual bool setDestAddr( const QString& foreignAddress, unsigned short foreignPort ) override;
    //!Implementation of AbstractDatagramSocket::sendTo
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignAddress ) override;
    //!Implementation of AbstractCommunicatingSocket::recv
    /*!
        Actually calls \a UDPSocket::recvFrom and saves datagram source address/port
    */
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override;
    //!Implementation of AbstractDatagramSocket::recvFrom
    virtual int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        QString& sourceAddress,
        unsigned short& sourcePort ) override;
    //!Implementation of AbstractDatagramSocket::lastDatagramSourceAddress
    virtual SocketAddress lastDatagramSourceAddress() const override;
    //!Implementation of AbstractDatagramSocket::hasData
    virtual bool hasData() const override;
    //!Implementation of AbstractDatagramSocket::setMulticastIF
    /**
    *   Set the multicast send interface
    *   @param multicastIF multicast interface for sending packets
    */
    virtual bool setMulticastIF( const QString& multicastIF ) override;

private:
    sockaddr_in m_destAddr;
    SocketAddress m_prevDatagramAddress;

    void setBroadcast();
    /*!
        \param sourcePort Port is returned in host byte order
    */
    int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        HostAddress* const sourceAddress,
        int* const sourcePort );
};

#endif
