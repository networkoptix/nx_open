#ifndef NX_NETWORK_STREAM_SOCKET_WRAPPER_H
#define NX_NETWORK_STREAM_SOCKET_WRAPPER_H

#include <atomic>

#include "socket_attributes_cache.h"

namespace nx {
namespace network {

/** Provides a bocking mode @class AbstractStreamSocket on top of any nonblocking
 *  mode @class AbstractStreamSocket */
class NX_NETWORK_API StreamSocketWrapper:
    public AbstractStreamSocketAttributesCache<AbstractStreamSocket>
{
public:
    StreamSocketWrapper(std::unique_ptr<AbstractStreamSocket> socket);

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    void shutdown() override;
    bool isClosed() const override;
    bool setNonBlockingMode(bool val) override;
    bool getNonBlockingMode(bool* val) const override;

    //!Implementation of AbstractCommunicatingSocket::*
    bool connect(const SocketAddress& sa, unsigned int ms) override;
    int recv(void* buffer, unsigned int bufferLen, int flags) override;
    int send(const void* buffer, unsigned int bufferLen) override;
    SocketAddress getForeignAddress() const override;
    bool isConnected() const override;

    void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    void cancelIOAsync(
        aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    void cancelIOSync(aio::EventType eventType) override;

    //!Implementation of AbstractStreamSocket::*
    bool reopen() override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;

public:
    std::atomic<bool> m_nonBlockingMode;
    std::unique_ptr<AbstractStreamSocket> m_socket;
};

} // namespace network
} // namespace nx

#endif // NX_NETWORK_STREAM_SOCKET_WRAPPER_H
