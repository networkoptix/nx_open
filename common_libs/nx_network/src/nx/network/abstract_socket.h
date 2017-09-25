#pragma once

#include <chrono>
#include <cstdint> /* For std::uintptr_t. */
#include <functional>
#include <memory>

#include <nx/network/async_stoppable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "aio/event_type.h"
#include "buffer.h"
#include "nettools.h"
#include "socket_common.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {

class AbstractAioThread;

} // namespace aio
} // namespace network
} // namespace nx

/**
 * Base interface for sockets. Provides methods to set different socket configuration parameters.
 * Removing socket:
 * Socket can be safely removed while inside socket's aio thread 
 * (e.g., inside completion handler of any asynchronous operation).
 * If removing socket in different thread, then caller MUST 
 * cancel all ongoing asynchronous operations (including timers, posts etc)
 * on socket first using QnStoppableAsync::pleaseStop
 * NOTE: On any method failure use SystemError::getLastOSErrorCode() to get error code.
 */
class NX_NETWORK_API AbstractSocket:
    public QnStoppableAsync
{
public:
#ifdef _WIN32
    /** NOTE: this actually is the following define:
     * 
     * typedef SOCKET SOCKET_HANDLE 
     * 
     * But we don't want to include windows headers here.
     * Equivalence of these typedefs is checked via static_assert in system_socket.cpp.
     */
    typedef std::uintptr_t SOCKET_HANDLE;
#else
    typedef int SOCKET_HANDLE;
#endif

    /**
     * Bind to local address/port.
     * @return false on error. Use SystemError::getLastOSErrorCode() to get error code.
     */
    virtual bool bind(const SocketAddress& localAddress) = 0;
    bool bind(const QString& localAddress, unsigned short localPort);

    virtual SocketAddress getLocalAddress() const = 0;

    virtual bool close() = 0;
    virtual bool isClosed() const = 0;
    virtual bool shutdown() = 0;

    /**
     * Allows mutiple sockets to bind to same address and port.
     * @return false on error.
     */
    virtual bool setReuseAddrFlag(bool reuseAddr) = 0;
    virtual bool getReuseAddrFlag(bool* val) const = 0;

    /**
     * if val is true turns non-blocking mode on, else turns it off.
     * @return false on error.
     */
    virtual bool setNonBlockingMode(bool val) = 0;
    /**
     * Reads non-blocking mode flag
     * @param val Filled with non-blocking mode flag in case of success. In case of error undefined
     * @return false on error. 
     */
    virtual bool getNonBlockingMode(bool* val) const = 0;

    /**
     * Reads MTU (in bytes).
     * @return false on error. 
     */
    virtual bool getMtu(unsigned int* mtuValue) const = 0;

    /**
     * Set socket's send buffer size (in bytes).
     * @return false on error. 
     */
    virtual bool setSendBufferSize(unsigned int buffSize) = 0;
    /**
     * Reads socket's send buffer size (in bytes).
     * @return false on error. 
     */
    virtual bool getSendBufferSize(unsigned int* buffSize) const = 0;

    /**
     * Set socket's receive buffer (in bytes).
     * @return false on error. 
     */
    virtual bool setRecvBufferSize(unsigned int buffSize) = 0;
    /**
     * Reads socket's read buffer size (in bytes).
     * @return false on error. 
     */
    virtual bool getRecvBufferSize(unsigned int* buffSize) const = 0;

    /**
     * Change socket's receive timeout (in millis).
     * @param ms New timeout value. 0 - no timeout.
     * @return true if timeout has been changed.
     * By default, there is no timeout.
     */
    virtual bool setRecvTimeout(unsigned int millis) = 0;
    bool setRecvTimeout(std::chrono::milliseconds m) { return setRecvTimeout(m.count()); }
    /**
     * Get socket's receive timeout (in millis).
     * @param millis In case of error value is udefined.
     * @return false on error.
     */
    virtual bool getRecvTimeout(unsigned int* millis) const = 0;

    /**
     * Change socket's send timeout (in millis).
     * @param ms. New timeout value. 0 - no timeout.
     * @return true if timeout has been changed.
     * By default, there is no timeout.
     */
    virtual bool setSendTimeout(unsigned int ms) = 0;
    bool setSendTimeout(std::chrono::milliseconds m) { return setSendTimeout(m.count()); }
    /**
     * Get socket's send timeout (in millis).
     * @param millis In case of error value is udefined.
     * @return false on error. 
     */
    virtual bool getSendTimeout(unsigned int* millis) const = 0;

    /**
     * Get socket's last error code. 
     * Needed in case if poll returned socket with flag aio::etError.
     * @return true if read error code successfully, false otherwise.
     */
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const = 0;

    /**
     * Returns system-specific socket handle.
     * TODO: #ak remove this method after complete move to the new socket.
     */
    virtual SOCKET_HANDLE handle() const = 0;

    /**
     * @return Handle for PollSet.
     */
    virtual nx::network::Pollable* pollable() = 0;

    /**
     * Invoke handler from within aio thread sock is bound to.
     * NOTE: Call will always be queued. I.e., if called from handler running 
     *   in aio thread, it will be called after handler has returned.
     * NOTE: handler execution is cancelled if socket polling for every event is cancelled.
     */
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Call handler from within aio thread sock is bound to.
     * NOTE: If called in aio thread, handler will be called from within this method.
     *   Otherwise - queued like AbstractSocket::post does.
     * NOTE: handler execution is cancelled if socket polling for every event is cancelled.
     */
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Returns pointer to AIOThread this socket is bound to
     * NOTE: if socket is not bound to any thread yet, binds it automatically
     */
    virtual nx::network::aio::AbstractAioThread* getAioThread() const = 0;

    /**
     * Binds current socket to specified AIOThread.
     * @note Internal NX_ASSERT(false) in case if socket can not be bound to
     *   specified tread (e.g. it's already bound to different thread or
     *   certaind thread type is not the same).
     */
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) = 0;

    /**
     * @return true if this socket in own AIO thread.
     */
    virtual bool isInSelfAioThread() const;
};

using IoCompletionHandler = nx::utils::MoveOnlyFunc<
    void(SystemError::ErrorCode /*errorCode*/, std::size_t /*bytesTransferred*/)>;

/**
 * Interface for writing to/reading from socket.
 */
class NX_NETWORK_API AbstractCommunicatingSocket:
    public AbstractSocket
{
public:
    constexpr static const int kDefaultTimeoutMillis = 3000;
    constexpr static const int kNoTimeout = 0;

    /**
     * Establish connection to specified foreign address.
     * @param remoteSocketAddress remote address (IP address or name) and port.
     * @param timeoutMillis connection timeout, 0 - no timeout.
     * @return false if unable to establish connection.
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
    /**
     * Read into the given buffer up to bufferLen bytes data from this socket.
     * Call AbstractCommunicatingSocket::connect() 
     *   before calling AbstractCommunicatingSocket::recv().
     * @param buffer buffer to receive the data
     * @param bufferLen maximum number of bytes to read into buffer
     * @return number of bytes read, 0 for EOF, and -1 for error. 
     * NOTE: If socket is in non-blocking mode and non-blocking send is not possible, 
     *   method will return -1 and set error code to SystemError::wouldBlock.
     */
    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) = 0;
    /**
     * Write the given buffer to this socket.
     * Call AbstractCommunicatingSocket::connect() before calling AbstractCommunicatingSocket::send().
     * @param buffer buffer to be written.
     * @param bufferLen number of bytes from buffer to be written.
     * @return Number of bytes sent. -1 if failed to send something. 
     * NOTE: If socket is in non-blocking mode and non-blocking send is not possible, 
     *   method will return -1 and set error code to SystemError::wouldBlock.
     */
    virtual int send(const void* buffer, unsigned int bufferLen) = 0;
    int send(const QByteArray& data);
    /**
     * Returns host address/port of remote host, socket has been connected to.
     * Get the foreign address. Call connect() before calling recv().
     * @return foreign address
     * NOTE: If AbstractCommunicatingSocket::connect() has not been called yet, 
     *   empty address is returned.
     */
    virtual SocketAddress getForeignAddress() const = 0;
    /**
     * @return Text name of the remote host. By default, getForeignAddress().address.toString().
     */
    virtual QString getForeignHostName() const;
    /**
     * Returns true, if connection has been established, false otherwise.
     */
    virtual bool isConnected() const = 0;

    /**
     * NOTE: sendTimeout is used.
     */
    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    /**
     * Reads bytes from socket asynchronously.
     * @param dst Buffer to read to. Maximum dst->capacity() bytes read to this buffer. 
     * If buffer already contains some data, newly-read data will be appended to it. 
     * Buffer is resized after reading to its actual size.
     * 
     * @param handler functor with following signature:
     *   @code{.cpp}
     *     (SystemError::ErrorCode errorCode, size_t bytesRead)
     *   @endcode
     *   bytesRead is undefined, if errorCode is not SystemError::noError.
     *   bytesRead is 0, if connection has been closed.
     * @return true, if asynchronous read has been issued
     * WARNING: If dst->capacity() == 0, false is returned and no bytes read.
     * WARNING: Multiple concurrent asynchronous write operations result in undefined behavior.
     */
    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Reads at least @param minimalSize bytes from socket asynchronously.
     * Calls @param handler when least @param minimalSize bytes are read or
     *   error or disconnect has happend.
     * NOTE: Works similar to POSIX MSG_WAITALL flag but async.
     */
    void readAsyncAtLeast(
        nx::Buffer* const buffer, size_t minimalSize,
        IoCompletionHandler handler);

    /**
     * Asynchnouosly writes all bytes from input buffer.
     * @param buf Calling party MUST guarantee that this object is not freed until send completion
     * @param handler functor with following parameters:
     * @code{.cpp}
     *   (SystemError::ErrorCode errorCode, size_t bytesWritten)
     * @endcode
     * bytesWritten differ from src size only if errorCode is not SystemError::noError.
     */
    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Register timer on this socket.
     * @param handler functor with no parameters.
     * NOTE: timeoutMs MUST be greater then zero!
     */
    virtual void registerTimer(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> handler) = 0;
    void registerTimer(
        unsigned int timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler);

    /**
     * Cancel async socket operation. cancellationDoneHandler is invoked when cancelled.
     * @param eventType event to cancel.
     */
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Cancels async operation and blocks until cancellation is stopped.
     * NOTE: It is guaranteed that no handler with eventType is running 
     *   or will be called after return of this method.
     * NOTE: If invoked within socket's aio thread, cancels immediately, without blocking.
     */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) = 0;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual QString idForToStringFromPtr() const; //< Used by toString(const T*).

private:
    void readAsyncAtLeastImpl(
        nx::Buffer* const buffer, size_t minimalSize,
        IoCompletionHandler handler,
        size_t initBufSize);
};

struct NX_NETWORK_API StreamSocketInfo
{
    /** Round-trip time smoothed variation, millis. */
    unsigned int rttVar = 0;
};

struct NX_NETWORK_API KeepAliveOptions
{
    std::chrono::seconds inactivityPeriodBeforeFirstProbe;
    std::chrono::seconds probeSendPeriod;
    /**
     * The number of unacknowledged probes to send before considering the connection dead and
     * notifying the application layer.
     */
    size_t probeCount;

    KeepAliveOptions(
        std::chrono::seconds inactivityPeriodBeforeFirstProbe = std::chrono::seconds::zero(),
        std::chrono::seconds probeSendPeriod = std::chrono::seconds::zero(),
        size_t probeCount = 0);

    bool operator==(const KeepAliveOptions& rhs) const;

    /** Maximum time before lost connection can be acknowledged. */
    std::chrono::seconds maxDelay() const;
    QString toString() const;
    static boost::optional<KeepAliveOptions> fromString(const QString& string);
};

/**
 * Interface for connection-orientied sockets.
 */
class NX_NETWORK_API AbstractStreamSocket:
    public AbstractCommunicatingSocket
{
public:
    /**
     * Reopenes previously closed socket.
     * TODO #ak this class is not a right place for this method.
     */
    virtual bool reopen() = 0;
    /**
     * Set TCP_NODELAY option (disable data aggregation).
     * @return false on error. 
     */
    virtual bool setNoDelay(bool value) = 0;
    /**
     * Read TCP_NODELAY option value.
     * @return false on error. 
     */
    virtual bool getNoDelay(bool* value) const = 0;
    /**
     * Enable collection of socket statistics.
     * @param val true - enable, false - diable.
     * NOTE: This method MUST be called only after establishing connection. 
     *   After reconnecting socket, it MUST be called again!
     * NOTE: On win32 only process with admin rights can enable statistics collection, 
     *   on linux it is enabled by default for every socket.
     */
    virtual bool toggleStatisticsCollection(bool val) = 0;
    /**
     * Reads extended stream socket information.
     * NOTE: AbstractStreamSocket::toggleStatisticsCollection 
     *   MUST be called prior to this method.
     * NOTE: on win32 for tcp protocol this function is pretty slow, 
     *   so it is not recommended to call it too often.
     */
    virtual bool getConnectionStatistics(StreamSocketInfo* info) = 0;
    /**
     * Set keep alive options.
     * @return false on error. 
     * NOTE: due to some OS limitations some values might not be actually set eg
     *   linux: full support
     *   windows: only timeSec and intervalSec support
     *   macosx: only timeSec support
     *   other unix: only boolean enabled/disabled is supported
     */
    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) = 0;
    /**
     * Reads keep alive options.
     * @return false on error. 
     * NOTE: due to some OS limitations some values might be = 0 (meaning system defaults).
     */
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const = 0;
};

/**
 * Stream socket with encryption.
 * In most cases, AbstractStreamSocket interface is enough. 
 * This one is needed for SMTP/TLS, for example.
 */
class NX_NETWORK_API AbstractEncryptedStreamSocket:
    public AbstractStreamSocket
{
public:
    /** Has handshake has been initiated. */
    virtual bool isEncryptionEnabled() const = 0;
};

using AcceptCompletionHandler = 
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)>;

/**
 * Interface for server socket, accepting stream connections.
 * NOTE: This socket has default recv timeout of 250ms for backward compatibility.
 */
class NX_NETWORK_API AbstractStreamServerSocket:
    public AbstractSocket
{
public:
    static const int kDefaultBacklogSize = 128;

    /**
     * Start listening for incoming connections.
     * @param queueLen Size of queue of fully established connections 
     *   waiting for AbstractStreamServerSocket::accept(). 
     *   If queue is full and new connection arrives, it receives ECONNREFUSED error.
     * @return false on error. 
     * NOTE: Method returns immediately.
     */
    virtual bool listen(int backlog = kDefaultBacklogSize) = 0;
    /**
     * Accepts new connection.
     * @return NULL in case of error (use SystemError::getLastOSErrorCode() to get error description).
     * NOTE: Uses read timeout.
     */
    virtual AbstractStreamSocket* accept() = 0;
    /**
     * Starts async accept operation.
     * @param handler functor with following signature:
     * @code{.cpp}
     *   (SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection)
     *   //newConnection is nullptr in case of error
     * @endcode
     * newConnection is NULL, if errorCode is not SystemError::noError.
     */
    virtual void acceptAsync(AcceptCompletionHandler handler) = 0;
    /**
     * Cancel active AbstractStreamServerSocket::acceptAsync.
     */
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    /**
     * Cancel active AbstractStreamServerSocket::acceptAsync waiting for completion.
     * NOTE: If called within socket's aio thread, then does not block.
     */
    virtual void cancelIOSync() = 0;

    virtual QString idForToStringFromPtr() const; //< Used by toString(const T*).
};

static const QString BROADCAST_ADDRESS(QLatin1String("255.255.255.255"));

/**
 * Interface for connection-less socket.
 * In this case AbstractCommunicatingSocket::connect() just rememberes 
 * remote address to use with AbstractCommunicatingSocket::send().
 */
class NX_NETWORK_API AbstractDatagramSocket:
    public AbstractCommunicatingSocket
{
public:
    static const int UDP_HEADER_SIZE = 8;
    static const int MAX_IP_HEADER_SIZE = 60;
    static const int MAX_DATAGRAM_SIZE = 64*1024 - 1 - UDP_HEADER_SIZE - MAX_IP_HEADER_SIZE;

    /**
     * Set destination address for use by AbstractCommunicatingSocket::send() method.
     * Difference from AbstractCommunicatingSocket::connect() method is this method 
     *   does not enable filtering incoming datagrams by (foreignAddress, foreignPort),
     *   and AbstractCommunicatingSocket::connect() does.
     */
    virtual bool setDestAddr(const SocketAddress& foreignEndpoint) = 0;
    // TODO: #ak Drop following overload.
    bool setDestAddr(const QString& foreignAddress, unsigned short foreignPort);
    /**
     * Send the given buffer as a datagram to the specified address/port.
     * @param buffer buffer to be written
     * @param bufferLen number of bytes to write
     * @param foreignAddress address (IP address or name) to send to
     * @param foreignPort port number to send to
     * @return true if whole data has been sent
     * NOTE: Remebers new destination address 
     *   (as if AbstractDatagramSocket::setDestAddr(foreignAddress, foreignPort) has been called).
     */
    bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const QString& foreignAddress,
        unsigned short foreignPort);
    /**
     * Send the given buffer as a datagram to the specified address/port.
     * Same as previous method.
     */
    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignAddress) = 0;
    bool sendTo(
        const nx::Buffer& buf,
        const SocketAddress& foreignAddress);
    /**
     * @param completionHandler (errorCode, resolved target address, bytesSent).
     */
    virtual void sendToAsync(
        const nx::Buffer& buf,
        const SocketAddress& targetAddress,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) = 0;
    /**
     * Read up to bufferLen bytes data from this socket. 
     *   The given buffer is where the data will be placed.
     * @param buffer buffer to receive data
     * @param bufferLen maximum number of bytes to receive
     * @param sourceAddress If not nullptr datagram source endpoint is stored here.
     * @return number of bytes received and -1 in case of error.
     */
    virtual int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        SocketAddress* const sourceAddress) = 0;
    /**
     * @param buf Data appended here.
     * @param handler(errorCode, sourceAddress, bytesRead).
     */
    virtual void recvFromAsync(
        nx::Buffer* const buf,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) = 0;
    /**
     * @return Address of previous datagram read with 
     * AbstractCommunicatingSocket::recv or AbstractDatagramSocket::recvFrom.
     */
    virtual SocketAddress lastDatagramSourceAddress() const = 0;
    /**
     * Checks, whether data is available for reading in non-blocking mode. 
     * Does not block for timeout, returns immediately.
     * TODO: #ak Remove this method, since it requires use of select(), 
     *   which is heavy, use MSG_DONTWAIT instead.
     */
    virtual bool hasData() const = 0;
    /**
     * Set the multicast send interface.
     * @param multicastIF multicast interface for sending packets.
     */
    virtual bool setMulticastIF(const QString& multicastIF) = 0;

    /**
     * Join the specified multicast group.
     * @param multicastGroup multicast group address to join.
     */
    virtual bool joinGroup(const QString &multicastGroup) = 0;
    virtual bool joinGroup(const QString &multicastGroup, const QString& multicastIF) = 0;

    /**
     * Leave the specified multicast group.
     * @param multicastGroup multicast group address to leave.
     */
    virtual bool leaveGroup(const QString &multicastGroup) = 0;
    virtual bool leaveGroup(const QString &multicastGroup, const QString& multicastIF) = 0;
};
