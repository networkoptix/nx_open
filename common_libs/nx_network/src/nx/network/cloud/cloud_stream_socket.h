#pragma once

#include <queue>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/cpp14.h>

#include "nx/network/abstract_socket.h"
#include "nx/network/aio/basic_pollable.h"
#include "nx/network/socket_global.h"
#include "nx/network/socket_attributes_cache.h"
#include "any_accessible_address_connector.h"
#include "tunnel/tunnel_attributes.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Socket that is able to use hole punching (tcp or udp) and mediator to establish connection.
 * Method to use to connect to a remote peer is selected depending on the route to the peer.
 * If connection to peer requires using udp hole punching, then this socket uses UDT.
 * NOTE: Actual socket is instantiated only when address is known
 *   (AbstractCommunicatingSocket::connect or AbstractCommunicatingSocket::connectAsync)
 */
class NX_NETWORK_API CloudStreamSocket:
    public AbstractStreamSocketAttributesCache<AbstractStreamSocket>
{
    using BaseType = AbstractStreamSocketAttributesCache<AbstractStreamSocket>;

public:
    explicit CloudStreamSocket(int ipVersion = AF_INET);
    virtual ~CloudStreamSocket();

    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool isClosed() const override;
    virtual bool shutdown() override;
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;

    virtual bool connect(
        const SocketAddress& remoteAddress,
        std::chrono::milliseconds timeout) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    virtual void cancelIOAsync(
        aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync(aio::EventType eventType) override;

    virtual void post(nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual void connectAsync(
        const SocketAddress& address,
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

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual bool isInSelfAioThread() const override;
    virtual QString idForToStringFromPtr() const override;

    virtual QString getForeignHostName() const override;

private:
    typedef nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>>*
        SocketResultPrimisePtr;

    void connectToEntriesAsync(
        std::deque<AddressEntry> dnsEntries, int port,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    SystemError::ErrorCode applyRealNonBlockingMode(AbstractStreamSocket* streamSocket);

    void onConnectDone(
        SystemError::ErrorCode errorCode,
        boost::optional<TunnelAttributes> cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection);

    void cancelIoWhileInAioThread(aio::EventType eventType);
    void stopWhileInAioThread();

    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_socketDelegate;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectHandler;
    nx::utils::AsyncOperationGuard m_asyncConnectGuard;
    // TODO: #ak replace with aio::BasicPollable inheritance.
    aio::BasicPollable m_aioThreadBinder;
    aio::Timer m_timer;
    aio::BasicPollable m_readIoBinder;
    aio::BasicPollable m_writeIoBinder;
    std::atomic<SocketResultPrimisePtr> m_connectPromisePtr;
    TunnelAttributes m_cloudTunnelAttributes;
    std::unique_ptr<AnyAccessibleAddressConnector> m_multipleAddressConnector;

    QnMutex m_mutex;
    bool m_terminated;
    const int m_ipVersion;
};

} // namespace cloud
} // namespace network
} // namespace nx
