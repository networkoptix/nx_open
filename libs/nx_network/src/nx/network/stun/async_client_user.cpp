// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_client_user.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace stun {

AsyncClientUser::AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client):
    m_client(std::move(client))
{
    m_client->addOnReconnectedHandler(
        [this, guard = m_asyncGuard.sharedGuard()]()
        {
            if (auto lock = guard->lock())
                return post(std::bind(&AsyncClientUser::reportReconnect, this));
        },
        m_asyncGuard.sharedGuard().get());
}

AsyncClientUser::~AsyncClientUser()
{
    if (!m_terminated && isInSelfAioThread())
        stopWhileInAioThread();
}

SocketAddress AsyncClientUser::localAddress() const
{
    return m_client->localAddress();
}

SocketAddress AsyncClientUser::remoteAddress() const
{
    return m_client->remoteAddress();
}

void AsyncClientUser::sendRequest(
    Message request, AbstractAsyncClient::RequestHandler handler)
{
    m_client->sendRequest(
        std::move(request),
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)](
            SystemError::ErrorCode code, Message message) mutable
        {
            if (auto lock = guard->lock())
            {
                return post(
                    [handler = std::move(handler), code,
                        message = std::move(message)]() mutable
                    {
                        handler(code, std::move(message));
                    });
            }
        },
        m_asyncGuard.sharedGuard().get());
}

void AsyncClientUser::setIndicationHandler(
    int method, AbstractAsyncClient::IndicationHandler handler)
{
    return m_client->setIndicationHandler(
        method,
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)](
            Message message)
        {
            if (auto lock = guard->lock())
                return post(std::bind(std::move(handler), std::move(message)));
        },
        m_asyncGuard.sharedGuard().get());
}

void AsyncClientUser::setOnReconnectedHandler(
    AbstractAsyncClient::ReconnectHandler handler)
{
    m_reconnectHandler.swap(handler);
}

bool AsyncClientUser::setConnectionTimer(
    std::chrono::milliseconds period, AbstractAsyncClient::TimerHandler handler)
{
    return m_client->addConnectionTimer(
        period,
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)]()
        {
            if (auto lock = guard->lock())
                return post(std::move(handler));
        },
        m_asyncGuard.sharedGuard().get());
}

AbstractAsyncClient* AsyncClientUser::client() const
{
    return m_client.get();
}

void AsyncClientUser::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    disconnectFromClient();

    m_terminated = true;
}

void AsyncClientUser::disconnectFromClient()
{
    auto guard = m_asyncGuard.sharedGuard();
    auto guardPtr = guard.get();
    m_client->cancelHandlers(
        guardPtr,
        [guard = std::move(guard)]() mutable
        {
            // Guard shall be kept here up to the end of cancellation to prevent reuse of the
            // same address (new subscriptions might be accidently removed).
            guard.reset();
        });

    NX_VERBOSE(this, nx::format("Disconnected from client"));
    m_asyncGuard.reset();
}

void AsyncClientUser::reportReconnect()
{
    if (m_reconnectHandler)
        m_reconnectHandler();
}

} // namespace stun
} // namespace network
} // namespace nx
