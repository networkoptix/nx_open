#pragma once

#include "nx/network/socket_attributes_cache.h"

namespace nx {
namespace network {

/**
 * Stream socket wrapper with internal buffering to extend async functionality.
 */
class NX_NETWORK_API BufferedStreamSocket:
    public AbstractStreamSocketAttributesCache<AbstractStreamSocket>
{
public:
    BufferedStreamSocket(std::unique_ptr<AbstractStreamSocket> socket);

    /**
     * Handler will be called as soon as there is some data ready to recv.
     * @note: is not thread safe (conflicts with recv and recvAsync)
     */
    void catchRecvEvent(std::function<void(SystemError::ErrorCode)> handler);

    enum class Inject { only, replace, begin, end };

    /**
     * Injects data to internal buffer if we need to pass socket to another processor but read
     * more we could process ourserve.
     * @note: does not affect catchRecvEvent and is not thread safe (conflicts with recv and recvAsync)
     */
    void injectRecvData(Buffer buffer, Inject injectType = Inject::only);

    // TODO: Usefull if we want to reuse connection when user is done with it.
    // void setDoneHandler(nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)>);

    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    bool close() override;
    bool isClosed() const override;
    bool shutdown() override;
    bool reopen() override;

    bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis) override;

    int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    int send(const void* buffer, unsigned int bufferLen) override;
    SocketAddress getForeignAddress() const override;
    bool isConnected() const override;

    void cancelIOAsync(
        aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    void cancelIOSync(aio::EventType eventType) override;

    void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    Buffer m_internalRecvBuffer;
};

} // namespace network
} // namespace nx
