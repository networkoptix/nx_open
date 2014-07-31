/**********************************************************
* 28 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_SOCKET_H
#define ABSTRACT_SOCKET_H

#include <cstdint> /* For std::uintptr_t. */
#include <functional>
#include <memory>

#include <utils/common/byte_array.h>

#include "buffer.h"
#include "nettools.h"
#include "socket_common.h"
#include "utils/common/systemerror.h"


//todo: #ak cancel asynchoronous operations

//!Base interface for sockets. Provides methods to set different socket configuration parameters
class AbstractSocket
{
public:
#ifdef Q_OS_WIN
    /* Note: this actually is the following define:
     * 
     * typedef SOCKET SOCKET_HANDLE 
     * 
     * But we don't want to include windows headers here.
     * Equivalence of these typedefs is checked via static_assert in system_socket.cpp. */
    typedef std::uintptr_t SOCKET_HANDLE;
#else
    typedef int SOCKET_HANDLE;
#endif

    virtual ~AbstractSocket() {}

    //!Bind to local address/port
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool bind( const SocketAddress& localAddress ) = 0;
    bool bind( const QString& localAddress, unsigned short localPort ) { return bind( SocketAddress( localAddress, localPort ) ); };
    //!Bind to local network interface by its name
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    //virtual bool bindToInterface( const QnInterfaceAndAddr& iface ) = 0;
    //!Get socket address
    virtual SocketAddress getLocalAddress() const = 0;
    //!Get peer address
    virtual SocketAddress getPeerAddress() const = 0;
    //!Close socket
    virtual void close() = 0;
    //!Returns true, if socket has been closed previously with \a AbstractSocket::close call
    virtual bool isClosed() const = 0;

    //!Allows mutiple sockets to bind to same address and port
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setReuseAddrFlag( bool reuseAddr ) = 0;
    //!Reads reuse addr flag
    /*!
        \param val Filled with flag value in case of success. In case of error undefined
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getReuseAddrFlag( bool* val ) = 0;
    //!if \a val is \a true turns non-blocking mode on, else turns it off
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setNonBlockingMode( bool val ) = 0;
    //!Reads non-blocking mode flag
    /*!
        \param val Filled with non-blocking mode flag in case of success. In case of error undefined
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getNonBlockingMode( bool* val ) const = 0;
    //!Reads MTU (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getMtu( unsigned int* mtuValue ) = 0;
    //!Set socket's send buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setSendBufferSize( unsigned int buffSize ) = 0;
    //!Reads socket's send buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getSendBufferSize( unsigned int* buffSize ) = 0;
    //!Set socket's receive buffer (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setRecvBufferSize( unsigned int buffSize ) = 0;
    //!Reads socket's read buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getRecvBufferSize( unsigned int* buffSize ) = 0;
    //!Change socket's receive timeout (in millis)
    /*!
        \param ms. New timeout value. 0 - no timeout
        \return \a true if timeout has been changed
        By default, there is no timeout
    */
    virtual bool setRecvTimeout( unsigned int millis ) = 0;
    //!Get socket's receive timeout (in millis)
    /*!
        \param millis In case of error value is udefined
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getRecvTimeout( unsigned int* millis ) = 0;
    //!Change socket's send timeout (in millis)
    /*!
        \param ms. New timeout value. 0 - no timeout
        \return \a true if timeout has been changed
        By default, there is no timeout
    */
    virtual bool setSendTimeout( unsigned int ms ) = 0;
    //!Get socket's send timeout (in millis)
    /*!
        \param millis In case of error value is udefined
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getSendTimeout( unsigned int* millis ) = 0;
    //!Get socket's last error code. Needed in case of \a aio::etError
    /*!
        \return \a true if read error code successfully, \a false otherwise
    */
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) = 0;
    //!Returns system-specific socket handle
    /*!
        TODO: #ak remove this method after complete move to the new socket
    */
    virtual SOCKET_HANDLE handle() const = 0;
};

//!Interface for writing to/reading from socket
class AbstractCommunicatingSocket
:
    virtual public AbstractSocket
{
public:
    virtual ~AbstractCommunicatingSocket() {}

    static const int DEFAULT_TIMEOUT_MILLIS = 3000;

    //!Establish connection to specified foreign address
    /*!
        \param foreignAddress foreign address (IP address or name)
        \param foreignPort foreign port
        \param timeoutMillis connection timeout, 0 - no timeout
        \return false if unable to establish connection
     */
    virtual bool connect(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) = 0;
    //!Read into the given \a buffer up to \a bufferLen bytes data from this socket
    /*!
        Call \a AbstractCommunicatingSocket::connect() before calling \a AbstractCommunicatingSocket::recv()
        \param buffer buffer to receive the data
        \param bufferLen maximum number of bytes to read into buffer
        \param flags TODO
        \return number of bytes read, 0 for EOF, and -1 for error. Use \a SystemError::getLastOSErrorCode() to get error code
        \note If socket is in non-blocking mode and non-blocking send is not possible, method will return -1 and set error code to \a SystemError::wouldBlock
     */
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) = 0;
    //!Write the given buffer to this socket
    /*!
        Call \a AbstractCommunicatingSocket::connect() before calling \a AbstractCommunicatingSocket::send()
        \param buffer buffer to be written
        \param bufferLen number of bytes from buffer to be written
        \return Number of bytes sent. -1 if failed to send something. Use \a SystemError::getLastOSErrorCode() to get error code
        \note If socket is in non-blocking mode and non-blocking send is not possible, method will return -1 and set error code to \a SystemError::wouldBlock
    */
    virtual int send( const void* buffer, unsigned int bufferLen ) = 0;
    int send( const QByteArray& data )  { return send( data.constData(), data.size() ); }
    int send( const QnByteArray& data ) { return send( data.constData(), data.size() ); }
    //!Returns host address/port of remote host, socket has been connected to
    /*!
        Get the foreign address.  Call connect() before calling recv()
        \return foreign address
        \note If \a AbstractCommunicatingSocket::connect() has not been called yet, empty address is returned
    */
    virtual const SocketAddress getForeignAddress() = 0;
    //!Returns \a true, if connection has been established, \a false otherwise
    /*!
        TODO/IMPL give up this method, since it's unreliable
    */
    virtual bool isConnected() const = 0;

    //!Reads bytes from socket asynchronously
    /*!
        \param dst Buffer to read to. Maximum \a dst->capacity() bytes read to this buffer
        \param handler functor with following signature:
            \code{.cpp}
                ( SystemError::ErrorCode errorCode, size_t bytesRead )
            \endcode
            \a bytesRead is undefined, if errorCode is not SystemError::noError.
            \a bytesRead is 0, if connection has been closed
        \return true, if asynchronous read has been issued
        \warning If \a dst->capacity() == 0, \a false is returned and no bytes read
        \warning Multiple concurrent asynchronous write operations result in undefined behavour
    */
    template<class HandlerType>
        bool readSomeAsync( nx::Buffer* const dst, HandlerType handler )
        {
            return recvAsyncImpl( dst, std::function<void( SystemError::ErrorCode, size_t )>( std::move( handler ) ) );
        }

    //!Asynchnouosly writes all bytes from input buffer
    /*!
        \param handler functor with following parameters:
            \code{.cpp}
                ( SystemError::ErrorCode errorCode, size_t bytesWritten )
            \endcode
            \a bytesWritten differ from \a src size only if errorCode is not SystemError::noError
    */
    template<class HandlerType>
        bool sendAsync( const nx::Buffer& src, HandlerType handler )
        {
            return sendAsyncImpl( src, std::function<void( SystemError::ErrorCode, size_t )>( std::move( handler ) ) );
        }

protected:
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void(SystemError::ErrorCode, size_t)> handler ) = 0;
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void(SystemError::ErrorCode, size_t)> handler ) = 0;
};

//!Interface for connection-orientied sockets
class AbstractStreamSocket
:
    virtual public AbstractCommunicatingSocket
{
public:
    virtual ~AbstractStreamSocket() {}

    //!Reopenes previously closed socket
    /*!
        TODO #ak this class is not a right place for this method
    */
    virtual bool reopen() = 0;
    //!Set TCP_NODELAY option (disable data aggregation)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setNoDelay( bool value ) = 0;
    //!Read TCP_NODELAY option value
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getNoDelay( bool* value ) = 0;
};

//!Interface for server socket, accepting stream connections
/*!
    \note This socket has default recv timeout of 250ms for backward compatibility
*/
class AbstractStreamServerSocket
:
    virtual public AbstractSocket
{
public:
    //!This class for internal use only. MAY be removed or changed in future
    class AbstractAsyncAcceptHandler
    {
    public:
        virtual ~AbstractAsyncAcceptHandler() {}
        virtual void onNewConnection( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection ) = 0;
    };


    virtual ~AbstractStreamServerSocket() {}

    //!Start listening for incoming connections
    /*!
        \note Method returns immediately
        \param queueLen Size of queue of fully established connections waiting for \a AbstractStreamServerSocket::accept(). 
            If queue is full and new connection arrives, it receives ECONNREFUSED error
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool listen( int queueLen = 128 ) = 0;
    //!Accepts new connection
    /*!
        \return NULL in case of error (use \a SystemError::getLastOSErrorCode() to get error description)
        \note Uses read timeout
    */
    virtual AbstractStreamSocket* accept() = 0;
    //!Starts async accept operation
    /*!
        \param handler functor with following signature:
            \code{.cpp}
                ( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection )
                //\a newConnection is \a nullptr in case of error
            \endcode
            \a newConnection is NULL, if errorCode is not SystemError::noError
    */
    template<class HandlerType>
        bool acceptAsync( HandlerType handler )
        {
            return acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>( std::move(handler) ) );
        }

protected:
    /*!
        \param handler This SHOULD be freed by implementation
    */
    virtual bool acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )> handler ) = 0;
};

static const QString BROADCAST_ADDRESS(QLatin1String("255.255.255.255"));

//!Interface for connection-less socket
/*!
    In this case \a AbstractCommunicatingSocket::connect() just rememberes remote address to use with \a AbstractCommunicatingSocket::send()
*/
class AbstractDatagramSocket
:
    virtual public AbstractCommunicatingSocket
{
public:
    static const int UDP_HEADER_SIZE = 8;
    static const int MAX_IP_HEADER_SIZE = 60;
    static const int MAX_DATAGRAM_SIZE = 64*1024 - 1 - UDP_HEADER_SIZE - MAX_IP_HEADER_SIZE;

    virtual ~AbstractDatagramSocket() {}

    //!Set destination address for use by \a AbstractCommunicatingSocket::send() method
    /*!
        Difference from \a AbstractCommunicatingSocket::connect() method is this method does not enable filtering incoming datagrams by (\a foreignAddress, \a foreignPort),
            and \a AbstractCommunicatingSocket::connect() does
    */
    virtual bool setDestAddr( const QString& foreignAddress, unsigned short foreignPort ) = 0;
    //!Send the given \a buffer as a datagram to the specified address/port
    /*!
        \param buffer buffer to be written
        \param bufferLen number of bytes to write
        \param foreignAddress address (IP address or name) to send to
        \param foreignPort port number to send to
        \return true if whole data has been sent
        \note Remebers new destination address (as if \a AbstractDatagramSocket::setDestAddr( \a foreignAddress, \a foreignPort ) has been called)
    */
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const QString& foreignAddress,
        unsigned short foreignPort ) = 0;
    //!Send the given \a buffer as a datagram to the specified address/port
    /*!
        Same as previous method
    */
    bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignAddress ) { return sendTo( buffer, bufferLen, foreignAddress.address.toString(), foreignAddress.port ); }
    //!Read read up to \a bufferLen bytes data from this socket. The given \a buffer is where the data will be placed
    /*!
        \param buffer buffer to receive data
        \param bufferLen maximum number of bytes to receive
        \param sourceAddress address of datagram source
        \param sourcePort port of data source
        \return number of bytes received and -1 in case of error
    */
    virtual int recvFrom(
        void* buffer,
        int bufferLen,
        QString& sourceAddress,
        unsigned short& sourcePort ) = 0;
    //!Checks, whether data is available for reading in non-blocking mode. Does not block for timeout, returns immediately
    /*!
        TODO: #ak remove this method, since it requires use of \a select(), which is heavy, use \a MSG_DONTWAIT instead
    */
    virtual bool hasData() const = 0;
    //!Set the multicast send interface
    /*!
        \param multicastIF multicast interface for sending packets
    */
    virtual bool setMulticastIF( const QString& multicastIF ) = 0;
};

#endif  //ABSTRACT_SOCKET_H
