// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/stun/abstract_async_client.h>
#include <nx/utils/async_operation_guard.h>

namespace nx {
namespace network {
namespace stun {

/**
 * AsyncClient wrapper which lives in a designated AIO thread.
 * Used to share single AbstractAsyncClient instance between multiple users.
 * Can be stopped (to prevent async calls) while AsyncClient still running.
 */
class NX_NETWORK_API AsyncClientUser:
    public network::aio::Timer
{
    using base_type = network::aio::Timer;

public:
    AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client);
    virtual ~AsyncClientUser() override;

    AsyncClientUser(const AsyncClientUser&) = delete;
    AsyncClientUser(AsyncClientUser&&) = delete;
    AsyncClientUser& operator=(const AsyncClientUser&) = delete;
    AsyncClientUser& operator=(AsyncClientUser&&) = delete;

    /** Returns local connection address in case if client is connected to STUN server */
    SocketAddress localAddress() const;

    /** Returns STUN server address in case if client has one */
    SocketAddress remoteAddress() const;

    void sendRequest(Message request, AbstractAsyncClient::RequestHandler handler);
    void setIndicationHandler(int method, AbstractAsyncClient::IndicationHandler handler);
    /** Handler called just after broken tcp connection has been restored */
    void setOnReconnectedHandler(AbstractAsyncClient::ReconnectHandler handler);
    void setOnConnectionClosedHandler(AbstractAsyncClient::OnConnectionClosedHandler handler);

    /** Timer is called over and over again until connection is closed */
    bool setConnectionTimer(
        std::chrono::milliseconds period, AbstractAsyncClient::TimerHandler handler);

    /** Return AbstractAsyncClient for configuration purposes */
    AbstractAsyncClient* client() const;

protected:
    virtual void stopWhileInAioThread() override;

protected:
    void disconnectFromClient();
    void reportConnectionClosed(SystemError::ErrorCode reason);
    void reportReconnect();

    nx::utils::AsyncOperationGuard m_asyncGuard;
    std::shared_ptr<AbstractAsyncClient> m_client;
    AbstractAsyncClient::ReconnectHandler m_reconnectHandler;
    AbstractAsyncClient::OnConnectionClosedHandler m_connectionClosedHandler;
    bool m_terminated = false;
};

} // namespace stun
} // namespace network
} // namespace nx
