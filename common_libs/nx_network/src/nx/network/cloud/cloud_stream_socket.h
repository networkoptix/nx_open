/**********************************************************
* Jan 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/future.h>
#include <utils/common/cpp14.h>

#include "nx/network/abstract_socket.h"
#include "nx/network/socket_global.h"
#include "nx/network/socket_attributes_cache.h"


namespace nx {
namespace network {
namespace cloud {

//!Socket that is able to use hole punching (tcp or udp) and mediator to establish connection
/*!
    Method to use to connect to remote peer is selected depending on route to the peer
    If connection to peer requires using udp hole punching than this socket uses UDT.
    \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
*/
class NX_NETWORK_API CloudStreamSocket
:
    public AbstractStreamSocketAttributesCache<AbstractStreamSocket>
{
public:
    CloudStreamSocket();
    virtual ~CloudStreamSocket();

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

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

private:
    typedef nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>>* SocketResultPrimisePtr;

    int recvImpl(nx::Buffer* const buf);

    void onAddressResolved(
        std::shared_ptr<nx::utils::AsyncOperationGuard::SharedGuard> sharedOperationGuard,
        int remotePort,
        SystemError::ErrorCode osErrorCode,
        std::vector<AddressEntry> dnsEntries);
    bool startAsyncConnect(
        //const SocketAddress& originalAddress,
        std::vector<AddressEntry> dnsEntries,
        int port);
    void onCloudConnectDone(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> cloudConnection);

    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_socketDelegate;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectHandler;
    nx::utils::AsyncOperationGuard m_asyncConnectGuard;
    /** Used to tie this to aio thread.
    //TODO #ak replace with aio thread timer */
    std::unique_ptr<AbstractDatagramSocket> m_aioThreadBinder;
    std::atomic<SocketResultPrimisePtr> m_recvPromisePtr;
    std::atomic<SocketResultPrimisePtr> m_sendPromisePtr;

    QnMutex m_mutex;
    bool m_terminated;
};

} // namespace cloud
} // namespace network
} // namespace nx
