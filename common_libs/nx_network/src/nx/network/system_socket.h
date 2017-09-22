#pragma once

#include <string>
#include <type_traits>

#include <QtCore/QString>

#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#else
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include <nx/utils/system_error.h>

#include "abstract_socket.h"
#include "aio/pollable.h"
#include "nettools.h"
#include "socket_factory.h"
#include "system_socket_address.h"

namespace nx {
namespace network {

namespace aio {
template<class SocketType> class BaseAsyncSocketImplHelper;
template<class SocketType> class AsyncSocketImplHelper;
} // namespace aio

#ifdef _WIN32
typedef int socklen_t;
#endif

/**
 * Base class representing basic communication endpoint.
 */
template<typename SocketInterfaceToImplement>
class Socket:
    public SocketInterfaceToImplement,
    public nx::network::Pollable
{
    static_assert(
        std::is_base_of<AbstractSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractSocket as a template argument");

public:
    Socket(
        int type,
        int protocol,
        int ipVersion,
        CommonSocketImpl* impl = nullptr);
    Socket(
        int sockDesc,
        int ipVersion,
        CommonSocketImpl* impl = nullptr);

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;
    virtual ~Socket();

    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override;

    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual bool getRecvTimeout(unsigned int* millis) const override;
    virtual bool getSendTimeout(unsigned int* millis) const override;
    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual bool isInSelfAioThread() const override;

    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool shutdown() override;

    virtual bool isClosed() const override;
    virtual bool setReuseAddrFlag(bool reuseAddr) override;
    virtual bool getReuseAddrFlag(bool* val) const override;
    virtual bool setNonBlockingMode(bool val) override;
    virtual bool getNonBlockingMode(bool* val) const override;
    virtual bool getMtu(unsigned int* mtuValue) const override;
    virtual bool setSendBufferSize(unsigned int buffSize) override;
    virtual bool getSendBufferSize(unsigned int* buffSize) const override;
    virtual bool setRecvBufferSize(unsigned int buffSize) override;
    virtual bool getRecvBufferSize(unsigned int* buffSize) const override;
    virtual bool setRecvTimeout(unsigned int ms) override;
    virtual bool setSendTimeout(unsigned int ms) override;

    virtual Pollable* pollable() override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;

    bool createSocket(int type, int protocol);

protected:
    const int m_ipVersion;

private:
    bool m_nonBlockingMode;
};

/**
 * Socket that is able to connect, send, and receive.
 */
template<class SocketInterfaceToImplement>
class CommunicatingSocket:
    public Socket<SocketInterfaceToImplement>
{
    static_assert(
        std::is_base_of<AbstractCommunicatingSocket, SocketInterfaceToImplement>::value,
        "You MUST use class derived of AbstractCommunicatingSocket as a template argument");

    typedef CommunicatingSocket<SocketInterfaceToImplement> SelfType;

public:
    CommunicatingSocket(
        int type,
        int protocol,
        int ipVersion,
        CommonSocketImpl* sockImpl = nullptr);

    CommunicatingSocket(
        int newConnSD,
        int ipVersion,
        CommonSocketImpl* sockImpl = nullptr);

    virtual ~CommunicatingSocket();

    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = AbstractCommunicatingSocket::kDefaultTimeoutMillis) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        IoCompletionHandler handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    virtual bool close() override;
    virtual bool shutdown() override;

protected:
    bool connectToIp(const SocketAddress& remoteAddress, unsigned int timeoutMillis);

    std::unique_ptr<aio::AsyncSocketImplHelper<SelfType>> m_aioHelper;
    bool m_connected;
#ifdef WIN32
    WSAEVENT m_eventObject;
#endif
};

/**
 * Client tcp socket.
 */
class NX_NETWORK_API TCPSocket:
    public CommunicatingSocket<AbstractStreamSocket>
{
    typedef CommunicatingSocket<AbstractStreamSocket> base_type;

public:
    explicit TCPSocket(int ipVersion = AF_INET);
    virtual ~TCPSocket();

    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;
    TCPSocket(TCPSocket&&) = delete;
    TCPSocket& operator=(TCPSocket&&) = delete;

    virtual bool reopen() override;
    virtual bool setNoDelay(bool value) override;
    virtual bool getNoDelay(bool* value) const override;
    virtual bool toggleStatisticsCollection(bool val) override;
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override;
    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) override;
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const override;

private:
    friend class TCPServerSocketPrivate;

    #if defined(_WIN32)
        KeepAliveOptions m_keepAlive;
    #endif

    /** Used by TCPServerSocket class. */
    TCPSocket(int newConnSD, int ipVersion);
};

class NX_NETWORK_API TCPServerSocket:
    public Socket<AbstractStreamServerSocket>
{
    typedef Socket<AbstractStreamServerSocket> base_type;

public:
    explicit TCPServerSocket(int ipVersion = AF_INET);
    ~TCPServerSocket();

    TCPServerSocket(const TCPServerSocket&) = delete;
    TCPServerSocket& operator=(const TCPServerSocket&) = delete;
    TCPServerSocket(TCPServerSocket&&) = delete;
    TCPServerSocket& operator=(TCPServerSocket&&) = delete;

    /**
     * Blocks until a new connection is established on this socket or error.
     * @return new connection socket.
     */
    static int accept(int sockDesc);

    virtual bool listen(int queueLen = 128) override;
    virtual AbstractStreamSocket* accept() override;
    virtual void pleaseStop(nx::utils::MoveOnlyFunc< void() > handler) override;
    virtual void pleaseStopSync(bool assertIfCalledUnderLock = true) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

    AbstractStreamSocket* systemAccept();

private:
    bool setListen(int queueLen);
    void stopWhileInAioThread();
};

class NX_NETWORK_API UDPSocket:
    public CommunicatingSocket<AbstractDatagramSocket>
{
    typedef CommunicatingSocket<AbstractDatagramSocket> base_type;

public:
    static const unsigned int MAX_IP_DATAGRAM_LENGTH = 64 * 1024;
    static const unsigned int IP_HEADER_MAX_LENGTH = 24;
    static const unsigned int UDP_HEADER_LENGTH = 8;
    static const unsigned int MAX_PACKET_SIZE =
        MAX_IP_DATAGRAM_LENGTH - IP_HEADER_MAX_LENGTH - UDP_HEADER_LENGTH;

    explicit UDPSocket(int ipVersion = AF_INET);
    virtual ~UDPSocket() override;
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&&) = delete;
    UDPSocket& operator=(UDPSocket&&) = delete;

    virtual SocketAddress getForeignAddress() const override;

    /**
     * Send the given buffer as a UDP datagram to the specified address/port.
     * @param buffer buffer to be written.
     * @param bufferLen number of bytes to write.
     * @param foreignAddress address (IP address or name) to send to.
     * @param foreignPort port number to send to.
     * @return true if send is successful.
     */
    bool sendTo(const void *buffer, int bufferLen);

    /**
     * Set the multicast TTL.
     * @param multicastTTL multicast TTL.
     */
    bool setMulticastTTL(unsigned char multicastTTL) ;

    virtual bool joinGroup( const QString &multicastGroup ) override;
    virtual bool joinGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    virtual bool leaveGroup( const QString &multicastGroup ) override;
    virtual bool leaveGroup( const QString &multicastGroup, const QString& multicastIF ) override;

    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    virtual bool setDestAddr( const SocketAddress& foreignEndpoint ) override;

    virtual bool sendTo(
        const void* buffer,
        unsigned int bufferLen,
        const SocketAddress& foreignEndpoint ) override;
    virtual void sendToAsync(
        const nx::Buffer& buf,
        const SocketAddress& foreignAddress,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> completionHandler) override;
    /**
     * Actually calls UDPSocket::recvFrom and makes datagram source address/port
     *   available through UDPSocket::lastDatagramSourceAddress.
     */
    virtual int recv( void* buffer, unsigned int bufferLen, int flags ) override;
    virtual int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        SocketAddress* const sourceAddress ) override;
    virtual void recvFromAsync(
        nx::Buffer* const buf,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> handler) override;
    virtual SocketAddress lastDatagramSourceAddress() const override;
    virtual bool hasData() const override;
    /**
     * Sets the multicast send interface.
     * @param multicastIF multicast interface for sending packets.
     */
    virtual bool setMulticastIF( const QString& multicastIF ) override;

private:
    SystemSocketAddress m_destAddr;
    SocketAddress m_prevDatagramAddress;

    void setBroadcast();
    /**
     * @param sourcePort Port is returned in host byte order.
     */
    int recvFrom(
        void* buffer,
        unsigned int bufferLen,
        HostAddress* const sourceAddress,
        quint16* const sourcePort );
};

qint64 NX_NETWORK_API totalSocketBytesSent();

} // namespace network
} // namespace nx
