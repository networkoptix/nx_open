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
#include "tunnel/tunnel_attributes.h"

namespace nx {
namespace network {
namespace cloud {

//!Socket that is able to use hole punching (tcp or udp) and mediator to establish connection
/*!
    Method to use to connect to remote peer is selected depending on route to the peer
    If connection to peer requires using udp hole punching than this socket uses UDT.
    \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
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

    //!Implementation of AbstractSocket::*
    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool isClosed() const override;
    virtual bool shutdown() override;
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamSocket::*
    virtual bool reopen() override;

    //!Implementation of AbstractCommunicatingSocket::*
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    virtual void cancelIOAsync(
        aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync(aio::EventType eventType) override;

    //!Implementation of AbstractSocket::*
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler ) override;

    //!Implementation of AbstractCommunicatingSocket::*
    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual bool isInSelfAioThread() const override;

    virtual QString getForeignHostFullCloudName() const;

private:
    typedef nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>>*
        SocketResultPrimisePtr;

    void connectToEntriesAsync(
        std::deque<AddressEntry> dnsEntries, int port,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    void connectToEntryAsync(
        const AddressEntry& dnsEntry, int port,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    SystemError::ErrorCode applyRealNonBlockingMode(AbstractStreamSocket* streamSocket);
    void onDirectConnectDone(SystemError::ErrorCode errorCode);
    void onCloudConnectDone(
        SystemError::ErrorCode errorCode,
        TunnelAttributes cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> cloudConnection);

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

    QnMutex m_mutex;
    bool m_terminated;
    const int m_ipVersion;
};

} // namespace cloud
} // namespace network
} // namespace nx
