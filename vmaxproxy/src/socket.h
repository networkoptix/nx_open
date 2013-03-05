#ifndef __PRACTICALSOCKET_INCLUDED__
#define __PRACTICALSOCKET_INCLUDED__

#include <string>
#include <exception>

#include <QString>

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif


#define MAX_ERROR_MSG_LENGTH 1024

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

/**
 *   Base class representing basic communication endpoint
 */
class Socket {
public:
    /**
     *   Close and deallocate this socket
     */
    ~Socket();

    QString lastError() const;

    virtual void close();
    bool isClosed() const;

    /**
     *   Get the local address
     *   @return local address of socket
     *   @exception SocketException thrown if fetch fails
     */
    QString getLocalAddress() const;

    /**
     *   Get the peer address
     *   @return remove address of socket
     *   @exception SocketException thrown if fetch fails
     */
    QString getPeerAddress() const;
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

    int handle() const { return sockDesc; }

    bool setReuseAddrFlag(bool reuseAddr);
    bool setLocalAddressAndPort(const QString &localAddress, unsigned short localPort);
    bool setNonBlockingMode(bool val);
    //!Returns true, if in non-blocking mode
    bool isNonBlockingMode() const;
protected:
    int sockDesc;              // Socket descriptor
    QString m_lastError;

    Socket(int type, int protocol) ;
    Socket(int sockDesc);
    bool fillAddr(const QString &address, unsigned short port, sockaddr_in &addr);
    void createSocket(int type, int protocol);

private:
    bool m_nonBlockingMode;

    // Prevent the user from trying to use value semantics on this object
    Socket(const Socket &sock);
    void operator=(const Socket &sock);
};

/**
 *   Socket which is able to connect, send, and receive
 */
class CommunicatingSocket : public Socket {
public:
    static const int DEFAULT_TIMEOUT_MILLIS = 3000;

    /**
     *   Establish a socket connection with the given foreign
     *   address and port
     *   @param foreignAddress foreign address (IP address or name)
     *   @param foreignPort foreign port
     *   @param timeoutMs connection timeout. if < 0 - no timeout used. TODO should use write timeout for connection establishment
     *   @return false if unable to establish connection
     */
    bool connect(const QString &foreignAddress, unsigned short foreignPort, int timeoutMs = DEFAULT_TIMEOUT_MILLIS);
    void shutdown();
    virtual void close();
    /*!
        \param ms. New timeout value. 0 - no timeout
        \return true. if timeout has been changed
        By default, there is no timeout
    */
    bool setReadTimeOut( unsigned int ms );
    /*!
        \param ms. New timeout value. 0 - no timeout
        \return true. if timeout has been changed
        By default, there is no timeout
    */
    bool setWriteTimeOut( unsigned int ms );

    /**
     *   Write the given buffer to this socket.  Call connect() before
     *   calling send()
     *   @param buffer buffer to be written
     *   @param bufferLen number of bytes from buffer to be written
     *   @return Number of bytes sent. -1 if failed to send something
     *   If socket is in non-blocking mode and non-blocking send is not possible, method will return -1 and set error code to wouldBlock
     */
    int send(const void *buffer, int bufferLen) ;

    /**
     *   Read into the given buffer up to bufferLen bytes data from this
     *   socket.  Call connect() before calling recv()
     *   @param buffer buffer to receive the data
     *   @param bufferLen maximum number of bytes to read into buffer
     *   @return number of bytes read, 0 for EOF, and -1 for error
     *   @exception SocketException thrown if unable to receive data
     */
    int recv(void *buffer, int bufferLen, int flags = 0);

    /**
     *   Get the foreign address.  Call connect() before calling recv()
     *   @return foreign address
     *   @exception SocketException thrown if unable to fetch foreign address
     */
    QString getForeignAddress() ;

    /**
     *   Get the foreign port.  Call connect() before calling recv()
     *   @return foreign port
     *   @exception SocketException thrown if unable to fetch foreign port
     */
    unsigned short getForeignPort() ;

    bool isConnected() const { return mConnected; }

    bool setSendBufferSize(int buff_size);
    bool setReadBufferSize(int buff_size);

    //!if, \a val is \a true, turns non-blocking mode on, else turns it off
    /*!
        \return true, if set successfully
    */
protected:
    CommunicatingSocket(int type, int protocol) ;
    CommunicatingSocket(int newConnSD);

protected:
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;
    bool mConnected;
};

/**
 *   TCP socket for communication with other TCP sockets
 */
class TCPSocket : public CommunicatingSocket {
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

    bool reopen();

    int setNoDelay(bool value);
private:
    // Access for TCPServerSocket::accept() connection creation
    friend class TCPServerSocket;
    TCPSocket(int newConnSD);
};

/**
 *   TCP socket class for servers
 */
class TCPServerSocket : public Socket {
public:
    /**
     *   Construct a TCP socket for use with a server, accepting connections
     *   on the specified port on any interface
     *   @param localPort local port of server socket, a value of zero will
     *                   give a system-assigned unused port
     *   @param queueLen maximum queue length for outstanding
     *                   connection requests (default 5)
     *   @exception SocketException thrown if unable to create TCP server socket
     */
    TCPServerSocket(unsigned short localPort, int queueLen = 5)
    ;

    /**
     *   Construct a TCP socket for use with a server, accepting connections
     *   on the specified port on the interface specified by the given address
     *   @param localAddress local interface (address) of server socket
     *   @param localPort local port of server socket
     *   @param queueLen maximum queue length for outstanding
     *                   connection requests (default 5)
     *   @exception SocketException thrown if unable to create TCP server socket
     */
    TCPServerSocket(const QString &localAddress, unsigned short localPort,
                    int queueLen = 5, bool reuseAddr = false) ;

    /**
     *   Blocks until a new connection is established on this socket or error
     *   @return new connection socket
     *   @exception SocketException thrown if attempt to accept a new connection fails
     */
    TCPSocket *accept() ;

private:
    void setListen(int queueLen) ;
};

#endif
