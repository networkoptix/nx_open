// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_async_client.h"

namespace nx::network::stun {

class NX_NETWORK_API AsyncClientDelegate:
    public nx::network::stun::AbstractAsyncClient
{
    using base_type = nx::network::stun::AbstractAsyncClient;

public:
    AsyncClientDelegate(
        std::unique_ptr<nx::network::stun::AbstractAsyncClient> delegate);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(
        const nx::utils::Url& url,
        ConnectHandler handler) override;

    virtual void setIndicationHandler(
        int method, IndicationHandler handler, void* client = nullptr) override;

    virtual void addOnReconnectedHandler(
        ReconnectHandler handler, void* client = nullptr) override;

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler onConnectionClosedHandler) override;

    virtual void sendRequest(
        nx::network::stun::Message request,
        RequestHandler handler,
        void* client) override;

    virtual bool addConnectionTimer(
        std::chrono::milliseconds period,
        TimerHandler handler,
        void* client) override;

    virtual nx::network::SocketAddress localAddress() const override;

    virtual nx::network::SocketAddress remoteAddress() const override;

    virtual void closeConnection(SystemError::ErrorCode errorCode) override;

    virtual void cancelHandlers(
        void* client, utils::MoveOnlyFunc<void()> handler) override;

    virtual void cancelHandlersSync(void* client) override;

    virtual void setKeepAliveOptions(
        nx::network::KeepAliveOptions options) override;

protected:
    virtual void stopWhileInAioThread() override;

    nx::network::stun::AbstractAsyncClient& delegate();
    const nx::network::stun::AbstractAsyncClient& delegate() const;

private:
    std::unique_ptr<nx::network::stun::AbstractAsyncClient> m_delegate;
};

} // namespace nx::network::stun
