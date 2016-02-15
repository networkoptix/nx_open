/**********************************************************
* Jan 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/async_operation_guard.h>
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
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    bool isClosed() const override;
    virtual void shutdown() override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamSocket::*
    bool reopen() override;

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
        std::function<void()> handler) override;
    virtual void cancelIOSync(aio::EventType eventType) override;

    //!Implementation of AbstractSocket::*
    virtual void post( std::function<void()> handler ) override;
    virtual void dispatch( std::function<void()> handler ) override;

    //!Implementation of AbstractCommunicatingSocket::*
    virtual void connectAsync(
        const SocketAddress& address,
        std::function<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        std::function<void()> handler) override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

private:
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

    std::atomic_unique_ptr<AbstractStreamSocket> m_socketDelegate;
    std::function<void(SystemError::ErrorCode)> m_connectHandler;
    nx::utils::AsyncOperationGuard m_asyncConnectGuard;
    /** Used to tie this to aio thread.
    //TODO #ak replace with aio thread timer */
    std::unique_ptr<AbstractDatagramSocket> m_aioThreadBinder;
    std::atomic<std::promise<std::pair<SystemError::ErrorCode, size_t>>*> m_recvPromisePtr;
    std::atomic<std::promise<std::pair<SystemError::ErrorCode, size_t>>*> m_sendPromisePtr;
};

} // namespace cloud
} // namespace network
} // namespace nx
