// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/network/socket_global.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include "detail/cloud_stream_socket_connector.h"
#include "tunnel/tunnel_attributes.h"

namespace nx::network::cloud {

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
    using base_type = AbstractStreamSocketAttributesCache<AbstractStreamSocket>;

public:
    explicit CloudStreamSocket(int ipVersion = AF_INET);
    virtual ~CloudStreamSocket();

    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual bool getProtocol(int* protocol) const override;

    virtual bool bind(const SocketAddress& localAddress) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual bool close() override;
    virtual bool isClosed() const override;
    virtual bool shutdown() override;
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;
    virtual nx::network::Pollable* pollable() override;

    virtual bool connect(
        const SocketAddress& remoteAddress,
        std::chrono::milliseconds timeout) override;

    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, std::size_t bufferLen) override;
    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    virtual void post(nx::utils::MoveOnlyFunc<void()> handler ) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler ) override;

    virtual void connectAsync(
        const SocketAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer* buf,
        IoCompletionHandler handler) override;
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync() override;

    virtual bool isInSelfAioThread() const override;

    virtual std::string getForeignHostName() const override;

protected:
    virtual void cancelIoInAioThread(aio::EventType eventType) override;

private:
    using SocketResultPromisePtr = std::promise<SystemError::ErrorCode>*;

    SystemError::ErrorCode applyRealNonBlockingMode(AbstractStreamSocket* streamSocket);

    void saveConnectResult(
        SystemError::ErrorCode errorCode,
        std::optional<TunnelAttributes> cloudTunnelAttributes,
        std::unique_ptr<AbstractStreamSocket> connection);

    void stopWhileInAioThread();

    std::unique_ptr<detail::CloudStreamSocketConnector> m_connector;
    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_socketDelegate;
    // TODO: #akolesnikov replace with aio::BasicPollable inheritance.
    aio::BasicPollable m_aioThreadBinder;
    aio::Timer m_timer;
    aio::BasicPollable m_readIoBinder;
    aio::BasicPollable m_writeIoBinder;
    std::atomic<SocketResultPromisePtr> m_connectPromisePtr = nullptr;
    TunnelAttributes m_cloudTunnelAttributes;
    std::atomic<bool> m_isClosed = false;

    const int m_ipVersion;
};

} // namespace nx::network::cloud
