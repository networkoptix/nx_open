#ifndef NX_CC_CLOUD_STREAM_SOCKET_H
#define NX_CC_CLOUD_STREAM_SOCKET_H

#include <nx/utils/async_operation_guard.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace cc {

//!Socket that is able to use hole punching (tcp or udp) and mediator to establish connection
/*!
    Method to use to connect to remote peer is selected depending on route to the peer
    If connection to peer requires using udp hole punching than this socket uses UDT.
    \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
*/
class CloudStreamSocket
:
    public AbstractStreamSocket
{
public:
    CloudStreamSocket(bool natTraversal);

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    bool isClosed() const override;
    bool setReuseAddrFlag(bool reuseAddr) override;
    bool getReuseAddrFlag(bool* val) const override;
    bool setNonBlockingMode(bool val) override;
    bool getNonBlockingMode(bool* val) const override;
    bool getMtu(unsigned int* mtuValue) const override;
    bool setSendBufferSize(unsigned int buffSize) override;
    bool getSendBufferSize(unsigned int* buffSize) const override;
    bool setRecvBufferSize(unsigned int buffSize) override;
    bool getRecvBufferSize(unsigned int* buffSize) const override;
    bool setRecvTimeout(unsigned int millis) override;
    bool getRecvTimeout(unsigned int* millis) const override;
    bool setSendTimeout(unsigned int ms) override;
    bool getSendTimeout(unsigned int* millis) const override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamSocket::*
    bool reopen() override;
    bool setNoDelay(bool value) override;
    bool getNoDelay(bool* value) const override;
    bool toggleStatisticsCollection(bool val) override;
    bool getConnectionStatistics(StreamSocketInfo* info) override;
    bool setKeepAlive(boost::optional<KeepAliveOptions> info) override;
    bool getKeepAlive(boost::optional<KeepAliveOptions>* result) const override;

    //!Implementation of AbstractCommunicatingSocket::*
    bool connect(const SocketAddress& remoteAddress,
                 unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS) override;

    int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    int send(const void* buffer, unsigned int bufferLen) override;
    SocketAddress getForeignAddress() const override;
    bool isConnected() const override;

    void cancelIOAsync(aio::EventType eventType,
                        std::function<void()> handler) override;

protected:
    //!Implementation of AbstractSocket::*
    void postImpl( std::function<void()>&& handler ) override;
    void dispatchImpl( std::function<void()>&& handler ) override;

    //!Implementation of AbstractCommunicatingSocket::*
    void connectAsyncImpl(
        const SocketAddress& address,
        std::function<void(SystemError::ErrorCode)>&& handler) override;

    void recvAsyncImpl(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)>&& handler) override;

    void sendAsyncImpl(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)>&& handler) override;

    void registerTimerImpl(
        unsigned int timeoutMs,
        std::function<void()>&& handler) override;

private:
    bool startAsyncConnect(const SocketAddress& originalAddress,
                           std::vector<AddressEntry> dnsEntries);

    int recvImpl(nx::Buffer* const buf);
    bool tunnelConnect(String peerId, std::vector<CloudConnectType> ccTypes);

    bool m_nonBlockingMode;
    std::shared_ptr<StreamSocketOptions> m_socketOptions;
    std::unique_ptr<AbstractStreamSocket> m_socketDelegate;
    std::function<void(SystemError::ErrorCode)> m_connectHandler;
    nx::utils::AsyncOperationGuard m_asyncGuard;
};

} // namespace cc
} // namespace nx

#endif  //NX_CC_CLOUD_STREAM_SOCKET_H
