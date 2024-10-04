// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <cstdint> /* For std::uintptr_t. */
#include <functional>
#include <memory>
#include <string>

#include <nx/network/async_stoppable.h>
#include <nx/utils/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "aio/event_type.h"
#include "socket_common.h"

namespace nx::network {

class Pollable;

namespace aio { class AbstractAioThread; }

namespace deprecated {

static constexpr std::chrono::milliseconds kDefaultConnectTimeout = std::chrono::seconds(10);

} // namespace deprecated

static constexpr std::chrono::milliseconds kNoTimeout =
    std::chrono::milliseconds::zero();

/**
 * Using IANA assigned numbers for protocol when possible.
 */
namespace Protocol {

static constexpr int tcp = 6;
static constexpr int udp = 17;
static constexpr int udt = 256;

/**
 * Custom protocol numbers should start with incrementing this number.
 */
static constexpr int unassigned = 257;

} // namespace ProtocolType

/**
 * Base interface for sockets. Provides methods to set various socket configuration parameters.
 * Notes on deleting a socket with scheduled asynchronous I/O on it:
 * Socket can be safely removed while inside socket's AIO thread (e.g., inside completion handler
 * of any asynchronous operation).
 * If removing socket in different thread, then caller MUST cancel all ongoing asynchronous
 * operations (including timers, posts, etc...) first using QnStoppableAsync::pleaseStop.
 *
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
    using SOCKET_HANDLE = std::uintptr_t;
    static constexpr SOCKET_HANDLE kInvalidSocket = ~((SOCKET_HANDLE) 0);
#else
    using SOCKET_HANDLE = int;
    static constexpr SOCKET_HANDLE kInvalidSocket = -1;
#endif

    /**
     * Bind to local address/port.
     * @return false on error. Use SystemError::getLastOSErrorCode() to get error code.
     */
    virtual bool bind(const SocketAddress& localAddress) = 0;
    bool bind(const std::string& localAddress, unsigned short localPort);

    /**
     * If socket was not bound to some address explicitly, then the binding may be done on late as
     * the first socket usage (this is OS-dependent).
     * @return The local address the socket is bound to.
     */
    virtual SocketAddress getLocalAddress() const = 0;

    /**
     * Close socket.
     * NOTE: Cannot be used to cancel I/O running concurrently in other threads. An UB may occur
     * when doing so.
     * @return false if an error occurred when closing the socket.
     * NOTE: the socket is closed anyway, regardless of the return value.
     */
    virtual bool close() = 0;

    /**
     * @return true if the socket has been closed with a AbstractSocket::close call.
     */
    virtual bool isClosed() const = 0;

    /**
     * Shut down all socket's I/O without closing the socket.
     * It may be invoked concurrently with synchronous socket operations running concurrently in
     * other threads. Whether these concurrent operations return immediately or not is OS-dependent
     * and cannot be relied upon.
     *
     * WARNING: this method does not cancel asynchronous I/O.
     */
    virtual bool shutdown() = 0;

    /**
     * Allows multiple sockets to bind to same address and port EXCEPT
     * multiple active listening sockets on the same.
     * @return false on error.
     * NOTE: Same as SO_REUSEADDR option on linux.
     */
    virtual bool setReuseAddrFlag(bool value) = 0;
    virtual bool getReuseAddrFlag(bool* value) const = 0;

    /**
     * Allows multiple listening sockets on the same address and port.
     * @return false on error.
     * NOTE: Same as SO_REUSEPORT option on linux.
     * NOTE: Due to lack of support in win32 API on mswin this option actually does nothing.
     *   Only AbstractSocket::setReuseAddrFlag(true) is enough for reusing port.
     */
    virtual bool setReusePortFlag(bool value) = 0;
    virtual bool getReusePortFlag(bool* value) const = 0;

    /**
     * if val is true turns non-blocking mode on, else turns it off.
     * @return false on error.
     */
    virtual bool setNonBlockingMode(bool val) = 0;

    /**
     * Reads non-blocking mode flag.
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
    bool setRecvTimeout(std::chrono::milliseconds m) { return setRecvTimeout((unsigned int) m.count()); }

    /**
     * Get socket's receive timeout (in millis).
     * @param millis In case of error value is undefined.
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
    bool setSendTimeout(std::chrono::milliseconds m) { return setSendTimeout((unsigned int) m.count()); }

    /**
     * Get socket's send timeout (in millis).
     * @param millis In case of error value is undefined.
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
     * Set IPV6_V6ONLY socket option. Default value is false on EVERY platform.
     */
    virtual bool setIpv6Only(bool val) = 0;

    /**
     * @param protocol One of values from Protocol namespace.
     * Note that additional protocol support can be added.
     * @return false if protocol is not defined yet.
     * An implementation is allowed to choose actual protocol after socket creation.
     */
    virtual bool getProtocol(int* protocol) const = 0;

    /**
     * @return System specific socket handle.
     * TODO: #akolesnikov Remove this method after complete move to the new socket.
     */
    virtual SOCKET_HANDLE handle() const = 0;

    /**
     * @return Handle for PollSet.
     */
    virtual nx::network::Pollable* pollable() = 0;

    /**
     * Invoke handler from within aio thread sock is bound to.
     * NOTE: Call will always be queued. I.e., if called from handler running
     * in aio thread, it will be called after handler has returned.
     * NOTE: handler execution is cancelled if socket polling for every event is cancelled.
     */
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * Call handler from within AIO thread the socket is bound to.
     * Differs from AbstractSocket::post in the following: if called in AIO thread, handler will be
     * invoked immediately within this method. Otherwise,
     * <pre><code>
     * post(std::move(handler))
     * </code></pre>
     * is invoked.
     * NOTE: handler execution is cancelled if socket polling for every event is cancelled.
     */
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * @return Pointer to AIO thread this socket is bound to.
     * NOTE: If the socket is not bound to any thread yet, it is bound by this method.
     */
    virtual nx::network::aio::AbstractAioThread* getAioThread() const = 0;

    /**
     * Binds current socket to specified AioThread.
     * Cannot be used to re-bind socket to a different AIO thread if asynchronous I/O has already
     * been scheduled on the socket.
     * Cancel all asynchronous I/O first.
     */
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) = 0;

    /**
     * @return true if the method is invoked within this socket's AIO thread.
     */
    virtual bool isInSelfAioThread() const;
};

using IoCompletionHandler = nx::utils::MoveOnlyFunc<
    void(SystemError::ErrorCode /*errorCode*/, std::size_t /*bytesTransferred*/)>;

/**
 * Interface of a socket that supports receiving / sending data.
 */
class NX_NETWORK_API AbstractCommunicatingSocket:
    public AbstractSocket
{
public:
    /**
     * Establish connection to specified foreign address.
     * @param remoteSocketAddress remote address (IP address or name) and port.
     * @param timeout connection timeout, 0 - no timeout.
     * @return false if unable to establish connection. Use SystemSocket::getLastOSErrorCode() to
     * get the error code.
     */
    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) = 0;

    bool connect(
        const std::string& foreignAddress,
        unsigned short foreignPort,
        std::chrono::milliseconds timeout);

    /**
     * @return true, if connection has been established, false otherwise.
     */
    virtual bool isConnected() const = 0;

    /**
     * Connect to a given address asynchronously.
     * NOTE: the socket send timeout is used
     * NOTE: the asynchronous connect is cancelled with `AbstractCommunicatingSocket::cancelWrite()`
     */
    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    /**
     * Read into the given buffer up to bufferLen bytes from connected socket.
     * @param buffer buffer to receive the data
     * @param bufferLen maximum number of bytes to read into buffer
     * @return number of bytes read, 0 for EOF, and -1 for error.
     * NOTE: If socket is in non-blocking mode and non-blocking read is not possible,
     * method will return -1 and set error code to SystemError::wouldBlock.
     */
    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) = 0;

    /**
     * Write the given buffer to connected socket.
     * @param buffer buffer to be written.
     * @param bufferLen number of bytes from buffer to be written.
     * @return number of bytes sent. -1 if failed to send something.
     * NOTE: If socket is in non-blocking mode and non-blocking send is not possible,
     * method will return -1 and set error code to SystemError::wouldBlock.
     */
    virtual int send(const void* buffer, std::size_t bufferLen) = 0;
    int send(const nx::Buffer& data);

    /**
     * @returns Host address/port of remote host socket has been connected to.
     * NOTE: If AbstractCommunicatingSocket::connect() has not been called yet,
     *   empty address is returned.
     */
    virtual SocketAddress getForeignAddress() const = 0;

    /**
     * @return Text name of the remote host. By default, getForeignAddress().address.toString().
     */
    virtual std::string getForeignHostName() const;

    /**
     * Reads bytes from connected socket asynchronously.
     * @param dst Buffer to read to. Maximum dst->capacity() bytes read to this buffer.
     * If buffer already contains some data, newly read data will be appended to it.
     * Buffer is resized after reading to its actual size.
     *
     * @param handler functor with following signature:
     * @code{.cpp}
     *     (SystemError::ErrorCode errorCode, size_t bytesRead)
     * @endcode
     * bytesRead is undefined, if errorCode is not SystemError::noError.
     * bytesRead is 0, if connection has been closed.
     * WARNING: Multiple concurrent asynchronous write operations are not queued and result in an
     * undefined behavior.
     */
    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) = 0;

    /**
     * Reads at least minimalSize bytes from socket asynchronously.
     * Calls handler when least minimalSize bytes are read or an error or disconnect has happened.
     * NOTE: Works similar to POSIX MSG_WAITALL flag but async.
     */
    void readAsyncAtLeast(
        nx::Buffer* const buffer, std::size_t minimalSize,
        IoCompletionHandler handler);

    /**
     * Asynchronously writes all bytes from input buffer.
     * @param buf Calling party MUST guarantee that this object is alive until send completion
     * @param handler functor with following parameters:
     * @code{.cpp}
     *   (SystemError::ErrorCode errorCode, size_t bytesWritten)
     * @endcode
     * bytesWritten differ from src size only if errorCode is not SystemError::noError.
     */
    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Register timer on this socket.
     * @param handler functor to be called
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
     * @param handler Always invoked within socket's AIO thread.
     */
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) final;

    /**
     * Cancels scheduled async operations with a given eventType (e.g., aio::etRead) and blocks
     * until the cancellation is completed.
     * It is guaranteed that no handler with eventType is running or will be called after the
     * return of this method.
     *
     * NOTE: If invoked within socket's AIO thread, cancels immediately, without blocking.
     *
     * NOTE: If invoked within some other AIO thread, may lead to an assertion raised or a deadlock.
     * So, to cancel async operation while in an AIO thread different this socket's (getAioThread()),
     * use AbstractCommunicatingSocket::cancelIOAsync.
     */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) final;

    /**
     * See AbstractCommunicatingSocket::cancelIOSync() description.
     */
    void cancelRead() { cancelIOSync(aio::etRead); };

    /**
     * See AbstractCommunicatingSocket::cancelIOSync() description.
     */
    void cancelWrite() { cancelIOSync(aio::etWrite); };

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void pleaseStopSync() override;

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) = 0;

private:
    void readAsyncAtLeastImpl(
        nx::Buffer* const buffer, std::size_t minimalSize,
        IoCompletionHandler handler,
        std::size_t initBufSize);
};

struct NX_NETWORK_API StreamSocketInfo
{
    /** Round-trip time smoothed variation, millis. */
    unsigned int rttVar = 0;
};

/**
 * Interface for connection-oriented sockets.
 */
class NX_NETWORK_API AbstractStreamSocket:
    public AbstractCommunicatingSocket
{
public:
    virtual ~AbstractStreamSocket() override;

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
     * @param val true - enable, false - disable.
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
     * NOTE: due to some OS limitations some values might not be actually set.
     *   linux: full support
     *   windows: only timeSec and intervalSec support
     *   macosx: only timeSec support
     *   other unix: only boolean enabled/disabled is supported
     */
    virtual bool setKeepAlive(std::optional<KeepAliveOptions> info) = 0;

    /**
     * Reads keep alive options.
     * @return false on error.
     * NOTE: due to some OS limitations some values might be = 0 (meaning system defaults).
     */
    virtual bool getKeepAlive(std::optional<KeepAliveOptions>* result) const = 0;

    void setBeforeDestroyCallback(nx::utils::MoveOnlyFunc<void()> callback);

private:
    nx::utils::MoveOnlyFunc<void()> m_beforeDestroyCallback;
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
    /**
     * Has handshake has been initiated.
     */
    virtual bool isEncryptionEnabled() const = 0;

    /**
     * Similar to AbstractCommunicatingSocket::connectAsync uses socket's send timeout.
     */
    virtual void handshakeAsync(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    /**
     * @return "servername" TLS extension from ClientHello. Valid only for accepted socket.
     */
    virtual std::string serverName() const = 0;
};

using AcceptCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>)>;

/**
 * Interface for a server socket that accepts stream connections.
 * NOTE: This socket has default recv timeout of 250ms for backward compatibility.
 */
class NX_NETWORK_API AbstractStreamServerSocket:
    public AbstractSocket
{
public:
    static constexpr int kDefaultBacklogSize = 128;

    /**
     * Start listening for incoming connections.
     * @param backlog Size of queue of fully established connections
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
    virtual std::unique_ptr<AbstractStreamSocket> accept() = 0;

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
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) final;

    /**
     * Cancel active AbstractStreamServerSocket::acceptAsync waiting for completion.
     * NOTE: If called within socket's aio thread, then does not block.
     */
    virtual void cancelIOSync() final;

protected:
    virtual void cancelIoInAioThread() = 0;
};

static constexpr char BROADCAST_ADDRESS[] = "255.255.255.255";

/**
 * Interface for connectionless socket. In this case AbstractCommunicatingSocket::connect() just
 * remembers the remote address to use with AbstractCommunicatingSocket::send().
 */
class NX_NETWORK_API AbstractDatagramSocket:
    public AbstractCommunicatingSocket
{
public:
    static constexpr int UDP_HEADER_SIZE = 8;
    static constexpr int MAX_IP_HEADER_SIZE = 60;
    static constexpr int MAX_DATAGRAM_SIZE = 64*1024 - 1 - UDP_HEADER_SIZE - MAX_IP_HEADER_SIZE;

    /**
     * Set destination address for use by AbstractCommunicatingSocket::send() method.
     * Difference from AbstractCommunicatingSocket::connect() method is this method
     *   does not enable filtering incoming datagrams by (foreignAddress, foreignPort),
     *   and AbstractCommunicatingSocket::connect() does.
     */
    virtual bool setDestAddr(const SocketAddress& foreignEndpoint) = 0;

    // TODO: #akolesnikov Drop following overload.
    bool setDestAddr(const std::string& foreignAddress, unsigned short foreignPort);

    /**
     * Send the given buffer as a datagram to the specified address/port.
     * @param buffer buffer to be written
     * @param bufferLen number of bytes to write
     * @param foreignAddress address (IP address or name) to send to
     * @param foreignPort port number to send to
     * @return true if whole data has been sent
     * NOTE: Remembers new destination address
     *   (as if AbstractDatagramSocket::setDestAddr(foreignAddress, foreignPort) has been called).
     */
    bool sendTo(
        const void* buffer,
        std::size_t bufferLen,
        const std::string& foreignAddress,
        unsigned short foreignPort);

    /**
     * Send the given buffer as a datagram to the specified address/port.
     * Same as previous method.
     */
    virtual bool sendTo(
        const void* buffer,
        std::size_t bufferLen,
        const SocketAddress& foreignAddress) = 0;

    bool sendTo(
        const nx::Buffer& buf,
        const SocketAddress& foreignAddress);

    /**
     * @param completionHandler (errorCode, resolved target address, bytesSent).
     */
    virtual void sendToAsync(
        const nx::Buffer* buf,
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
        std::size_t bufferLen,
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
     * TODO: #akolesnikov Remove this method, since it requires use of select(),
     *   which is heavy, use MSG_DONTWAIT instead.
     */
    virtual bool hasData() const = 0;

    /**
     * Set the multicast send interface.
     * @param multicastIF multicast interface for sending packets.
     */
    virtual bool setMulticastIF(const std::string& multicastIF) = 0;

    /**
     * Join the specified multicast group.
     * @param multicastGroup multicast group address to join.
     */
    virtual bool joinGroup(const HostAddress& multicastGroup) = 0;
    virtual bool joinGroup(const HostAddress& multicastGroup, const HostAddress& multicastIF) = 0;

    /**
     * Leave the specified multicast group.
     * @param multicastGroup multicast group address to leave.
     */
    virtual bool leaveGroup(const HostAddress& multicastGroup) = 0;
    virtual bool leaveGroup(const HostAddress& multicastGroup, const HostAddress& multicastIF) = 0;
};

} // namespace nx::network
