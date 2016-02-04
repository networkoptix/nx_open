#ifndef NX_NETWORK_STREAM_SOCKET_WRAPPER_H
#define NX_NETWORK_STREAM_SOCKET_WRAPPER_H

#include <atomic>

#include "abstract_socket.h"

namespace nx {
namespace network {

/** Provides a bocking mode @class AbstractStreamSocket on top of any nonblocking
 *  mode @class AbstractStreamSocket */
class NX_NETWORK_API StreamSocketWrapper
    : public AbstractStreamSocket
{
public:
    StreamSocketWrapper(std::unique_ptr<AbstractStreamSocket> socket);

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    void shutdown() override;
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
    aio::AbstractAioThread* getAioThread() override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractCommunicatingSocket::*
    bool connect(const SocketAddress& sa, unsigned int ms) override;
    int recv(void* buffer, unsigned int bufferLen, int flags) override;
    int send(const void* buffer, unsigned int bufferLen) override;
    SocketAddress getForeignAddress() const override;
    bool isConnected() const override;

    void connectAsync(
        const SocketAddress& addr,
        std::function<void(SystemError::ErrorCode)> handler) override;

    void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    void registerTimer(unsigned int timeoutMs, std::function<void()> handler) override;
    void cancelIOAsync(aio::EventType eventType, std::function<void()> handler) override;
    void cancelIOSync(aio::EventType eventType) override;

    //!Implementation of AbstractStreamSocket::*
    bool reopen() override;
    bool setNoDelay(bool value) override;
    bool getNoDelay(bool* value) const override;
    bool toggleStatisticsCollection(bool val) override;
    bool getConnectionStatistics(StreamSocketInfo* info) override;
    bool setKeepAlive(boost::optional<KeepAliveOptions> info) override;
    bool getKeepAlive(boost::optional<KeepAliveOptions>* result) const override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(std::function<void()> handler) override;
    void dispatch(std::function<void()> handler) override;

public:
    std::atomic<bool> m_nonBlockingMode;
    std::unique_ptr<AbstractStreamSocket> m_socket;
};

} // namespace network
} // namespace nx

#endif // NX_NETWORK_STREAM_SOCKET_WRAPPER_H
