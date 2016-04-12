/**********************************************************
* 28 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_SOCKET_H
#define ABSTRACT_SOCKET_H

#include <chrono>
#include <cstdint> /* For std::uintptr_t. */
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <utils/common/byte_array.h>
#include <utils/common/systemerror.h>
#include <utils/common/stoppable.h>

#include "aio/pollset.h"
#include "buffer.h"
#include "nettools.h"
#include "socket_common.h"

//todo: #ak cancel asynchoronous operations

// forward
namespace nx {
namespace network {
namespace aio {

class AbstractAioThread;

}   //aio
}   //network
}   //nx

/** Base interface for sockets. Provides methods to set different socket configuration parameters.

    \par Removing socket:
        Socket can be safely removed while inside socket's aio thread 
    (e.g., inside completion handler of any asynchronous operation).
        If removing socket in different thread, then caller MUST 
    cancel all ongoing asynchronous operations (including timers, posts etc)
    on socket first using \a QnStoppableAsync::pleaseStop
*/
class NX_NETWORK_API AbstractSocket
    : public QnStoppableAsync
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

    //!Bind to local address/port
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool bind( const SocketAddress& localAddress ) = 0;
    bool bind( const QString& localAddress, unsigned short localPort );
    //!Bind to local network interface by its name
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    //virtual bool bindToInterface( const QnInterfaceAndAddr& iface ) = 0;
    //!Get local address, socket is bound to
    virtual SocketAddress getLocalAddress() const = 0;
    //!Close socket
    virtual bool close() = 0;
    //!Returns true, if socket has been closed previously with \a AbstractSocket::close call
    virtual bool isClosed() const = 0;

    //!Shutdown socket
    virtual bool shutdown() = 0;

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
    virtual bool getReuseAddrFlag( bool* val ) const = 0;
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
    virtual bool getMtu( unsigned int* mtuValue ) const = 0;
    //!Set socket's send buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setSendBufferSize( unsigned int buffSize ) = 0;
    //!Reads socket's send buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getSendBufferSize( unsigned int* buffSize ) const = 0;
    //!Set socket's receive buffer (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool setRecvBufferSize( unsigned int buffSize ) = 0;
    //!Reads socket's read buffer size (in bytes)
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
    */
    virtual bool getRecvBufferSize( unsigned int* buffSize ) const = 0;
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
    virtual bool getRecvTimeout( unsigned int* millis ) const = 0;
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
    virtual bool getSendTimeout( unsigned int* millis ) const = 0;
    //!Get socket's last error code. Needed in case of \a aio::etError
    /*!
        \return \a true if read error code successfully, \a false otherwise
    */
    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const = 0;
    //!Returns system-specific socket handle
    /*!
        TODO: #ak remove this method after complete move to the new socket
    */
    virtual SOCKET_HANDLE handle() const = 0;
    //!Call \a handler from within aio thread \a sock is bound to
    /*!
        \note Call will always be queued. I.e., if called from handler running in aio thread, it will be called after handler has returned
        \note \a handler execution is cancelled if socket polling for every event is cancelled
    */
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    //!Call \a handler from within aio thread \a sock is bound to
    /*!
        \note If called in aio thread, handler will be called from within this method, otherwise - queued like \a AbstractSocket::post does
        \note \a handler execution is cancelled if socket polling for every event is cancelled
    */
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    //!Returns pointer to AIOThread this socket is bound to
    /*!
        \note if socket is not bound to any thread yet, binds it automatically
    */
    virtual nx::network::aio::AbstractAioThread* getAioThread() = 0;

    //!Binds current socket to specified AIOThread
    /*!
        \note internal NX_ASSERT(false) in case if socket can not be bound to
              specified tread (e.g. it's already bound to different thread or
              certaind thread type is not the same)
     */
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) = 0;
};

//!Interface for writing to/reading from socket
class NX_NETWORK_API AbstractCommunicatingSocket
:
    public AbstractSocket
{
public:
    constexpr static const int kDefaultTimeoutMillis = 3000;
    constexpr static const int kNoTimeout = 0;

    //!Establish connection to specified foreign address
    /*!
        \param remoteSocketAddress remote address (IP address or name) and port
        \param timeoutMillis connection timeout, 0 - no timeout
        \return false if unable to establish connection
    */
    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) = 0;
    bool connect(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = kDefaultTimeoutMillis);
    bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeoutMillis);
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
    int send( const QByteArray& data );
    int send( const QnByteArray& data );
    //!Returns host address/port of remote host, socket has been connected to
    /*!
        Get the foreign address.  Call connect() before calling recv()
        \return foreign address
        \note If \a AbstractCommunicatingSocket::connect() has not been called yet, empty address is returned
    */
    virtual SocketAddress getForeignAddress() const = 0;
    //!Returns \a true, if connection has been established, \a false otherwise
    /*!
        TODO/IMPL give up this method, since it's unreliable
    */
    virtual bool isConnected() const = 0;

    /*!
        \note uses sendTimeout
    */
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    //!Reads bytes from socket asynchronously
    /*!
        \param dst Buffer to read to. Maximum \a dst->capacity() bytes read to this buffer. If buffer already contains some data, 
            newly-read data will be appended to it. Buffer is resized after reading to its actual size
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
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;

    //!Reads at least @param minimalSize bytes from socket asynchronously
    void readFixedAsync(
        nx::Buffer* const buf, size_t minimalSize,
        std::function<void(SystemError::ErrorCode, size_t)> handler,
        size_t baseSize = 0);

    //!Asynchnouosly writes all bytes from input buffer
    /*!
        \param buf Calling party MUST guarantee that this object is not freed until send completion
        \param handler functor with following parameters:
            \code{.cpp}
                ( SystemError::ErrorCode errorCode, size_t bytesWritten )
            \endcode
            \a bytesWritten differ from \a src size only if errorCode is not SystemError::noError
    */
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;

    //!Register timer on this socket
    /*!
        \param handler functor with no parameters
        \note \a timeoutMs MUST be greater then zero!
    */
    virtual void registerTimer(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> handler) = 0;
    void registerTimer(
        unsigned int timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler);

    //!Cancel async socket operation. \a cancellationDoneHandler is invoked when cancelled
    /*!
        \param eventType event to cancel
    */
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc< void() > handler) = 0;

    //!Cancels async operation and blocks until cancellation is stopped
    /*!
        \note It is guaranteed that no handler with \a eventType is running or will be called after return of this method
        \note If invoked within socket's aio thread, cancels immediately, without blocking
    */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) = 0;

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    //!Implementation of QnStoppable::pleaseStopSync
    virtual void pleaseStopSync() override;
};

struct NX_NETWORK_API StreamSocketInfo
{
    //!round-trip time smoothed variation, millis
    unsigned int rttVar;

    StreamSocketInfo()
    :
        rttVar( 0 )
    {
    }
};

struct NX_NETWORK_API KeepAliveOptions
{
    /** timeout, in seconds, with no activity until the first keep-alive
     *  packet is sent */
    int timeSec;

    /** interval, in seconds, between when successive keep-alive packets
     *  are sent if no acknowledgement is received */
    int intervalSec;

    /** the number of unacknowledged probes to send before considering
     *  the connection dead and notifying the application layer */
    int probeCount;

    KeepAliveOptions( int time = 0, int interval = 0, int probes = 0 )
        : timeSec( time ), intervalSec( interval ), probeCount( probes ) {}
};

//!Interface for connection-orientied sockets
class NX_NETWORK_API AbstractStreamSocket
:
    public AbstractCommunicatingSocket
{
public:
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
    virtual bool getNoDelay( bool* value ) const = 0;
    //!Enable collection of socket statistics
    /*!
        \param val \a true - enable, \a false - diable
        \note This method MUST be called only after establishing connection. After reconnecting socket, it MUST be called again!
        \note On win32 only process with admin rights can enable statistics collection, on linux it is enabled by default for every socket
    */
    virtual bool toggleStatisticsCollection( bool val ) = 0;
    //!Reads extended stream socket information
    /*!
        \note \a AbstractStreamSocket::toggleStatisticsCollection MUST be called prior to this method
        \note on win32 for tcp protocol this function is pretty slow, so it is not recommended to call it too often
    */
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) = 0;
    //!Set keep alive options
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
        \note due to some OS limitations some values might not be actually set eg
            windows: probeCount is not supported
            macosx: only boolean enabled/disabled is supported
    */
    virtual bool setKeepAlive( boost::optional< KeepAliveOptions > info ) = 0;
    //!Reads keep alive options
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() to get error code
        \note due to some OS limitations some values might be = 0 (meaning system defaults)
    */
    virtual bool getKeepAlive( boost::optional< KeepAliveOptions >* result ) const = 0;
};

//!Stream socket with encryption
/*!
    In most cases, \a AbstractStreamSocket interface is enough. This one is needed for SMTP/TLS, for example
*/
class NX_NETWORK_API AbstractEncryptedStreamSocket
:
    public AbstractStreamSocket
{
public:
    //!Connect to remote host without performing SSL handshake
    /*!
        \a AbstractCommunicatingSocket::connect connects and performs handshake. This method is required for SMTP/TLS, for example
    */
    virtual bool connectWithoutEncryption(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) = 0;
    //!Do SSL handshake and use encryption on succeeding data exchange
    virtual bool enableClientEncryption() = 0;
};

//!Interface for server socket, accepting stream connections
/*!
    \note This socket has default recv timeout of 250ms for backward compatibility
*/
class NX_NETWORK_API AbstractStreamServerSocket
:
    public AbstractSocket
{
public:
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
    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler) = 0;
    /** Cancel active \a AbstractStreamServerSocket::acceptAsync */
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    /** Cancel active \a AbstractStreamServerSocket::acceptAsync waiting for completion.
        \note If called within socket's aio thread, then does not block
    */
    virtual void cancelIOSync() = 0;
};

static const QString BROADCAST_ADDRESS(QLatin1String("255.255.255.255"));

//!Interface for connection-less socket
/*!
    In this case \a AbstractCommunicatingSocket::connect() just rememberes remote address to use with \a AbstractCommunicatingSocket::send()
*/
class NX_NETWORK_API AbstractDatagramSocket
:
    public AbstractCommunicatingSocket
{
public:
    static const int UDP_HEADER_SIZE = 8;
    static const int MAX_IP_HEADER_SIZE = 60;
    static const int MAX_DATAGRAM_SIZE = 64*1024 - 1 - UDP_HEADER_SIZE - MAX_IP_HEADER_SIZE;

    //!Set destination address for use by \a AbstractCommunicatingSocket::send() method
    /*!
        Difference from \a AbstractCommunicatingSocket::connect() method is this method does not enable filtering incoming datagrams by (\a foreignAddress, \a foreignPort),
            and \a AbstractCommunicatingSocket::connect() does
    */
    virtual bool setDestAddr( const SocketAddress& foreignEndpoint ) = 0;
    //TODO #ak drop following overload
    bool setDestAddr( const QString& foreignAddress, unsigned short foreignPort );
    //!Send the given \a buffer as a datagram to the specified address/port
    /*!
        \param buffer buffer to be written
        \param bufferLen number of bytes to write
        \param foreignAddress address (IP address or name) to send to
        \param foreignPort port number to send to
        \return true if whole data has been sent
        \note Remebers new destination address (as if \a AbstractDatagramSocket::setDestAddr( \a foreignAddress, \a foreignPort ) has been called)
    */
    bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const QString& foreignAddress,
        unsigned short foreignPort );
    //!Send the given \a buffer as a datagram to the specified address/port
    /*!
        Same as previous method
    */
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignAddress ) = 0;
    bool sendTo(
        const nx::Buffer& buf,
        const SocketAddress& foreignAddress);
    /*!
        \param completionHandler (errorCode, resolved target address, bytesSent)
    */
    virtual void sendToAsync(
        const nx::Buffer& buf,
        const SocketAddress& targetAddress,
        std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) = 0;
    //!Read up to \a bufferLen bytes data from this socket. The given \a buffer is where the data will be placed
    /*!
        \param buffer buffer to receive data
        \param bufferLen maximum number of bytes to receive
        \param sourceAddress If not \a nullptr datagram source endpoint is stored here
        \return number of bytes received and -1 in case of error
    */
    virtual int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        SocketAddress* const sourceAddress ) = 0;
    /*!
        \param buf Data appended here
        \param handler(errorCode, sourceAddress, bytesRead)
    */
    virtual void recvFromAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) = 0;
    //!Returns address of previous datagram read with \a AbstractCommunicatingSocket::recv or \a AbstractDatagramSocket::recvFrom
    virtual SocketAddress lastDatagramSourceAddress() const = 0;
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

    /**
    *   Join the specified multicast group
    *   @param multicastGroup multicast group address to join
    */
    virtual bool joinGroup( const QString &multicastGroup ) = 0;
    virtual bool joinGroup( const QString &multicastGroup, const QString& multicastIF ) = 0;

    /**
    *   Leave the specified multicast group
    *   @param multicastGroup multicast group address to leave
    */
    virtual bool leaveGroup( const QString &multicastGroup ) = 0;
    virtual bool leaveGroup( const QString &multicastGroup, const QString& multicastIF ) = 0;
};

#endif  //ABSTRACT_SOCKET_H
