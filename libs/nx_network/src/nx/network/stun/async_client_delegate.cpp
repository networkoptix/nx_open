#include "async_client_delegate.h"

namespace nx::network::stun {

AsyncClientDelegate::AsyncClientDelegate(
    std::unique_ptr<nx::network::stun::AbstractAsyncClient> delegate)
    :
    m_delegate(std::move(delegate))
{
    if (m_delegate)
        bindToAioThread(m_delegate->getAioThread());
}

void AsyncClientDelegate::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_delegate->bindToAioThread(aioThread);
}

void AsyncClientDelegate::connect(
    const nx::utils::Url& url,
    ConnectHandler handler)
{
    m_delegate->connect(url, std::move(handler));
}

bool AsyncClientDelegate::setIndicationHandler(
    int method, IndicationHandler handler, void* client)
{
    return m_delegate->setIndicationHandler(method, std::move(handler), client);
}

void AsyncClientDelegate::addOnReconnectedHandler(
    ReconnectHandler handler, void* client)
{
    m_delegate->addOnReconnectedHandler(std::move(handler), client);
}

void AsyncClientDelegate::setOnConnectionClosedHandler(
    OnConnectionClosedHandler onConnectionClosedHandler)
{
    m_delegate->setOnConnectionClosedHandler(
        std::move(onConnectionClosedHandler));
}

void AsyncClientDelegate::sendRequest(
    nx::network::stun::Message request,
    RequestHandler handler,
    void* client)
{
    m_delegate->sendRequest(std::move(request), std::move(handler), client);
}

bool AsyncClientDelegate::addConnectionTimer(
    std::chrono::milliseconds period,
    TimerHandler handler,
    void* client)
{
    return m_delegate->addConnectionTimer(
        period, std::move(handler), client);
}

nx::network::SocketAddress AsyncClientDelegate::localAddress() const
{
    return m_delegate->localAddress();
}

nx::network::SocketAddress AsyncClientDelegate::remoteAddress() const
{
    return m_delegate->remoteAddress();
}

void AsyncClientDelegate::closeConnection(
    SystemError::ErrorCode errorCode)
{
    m_delegate->closeConnection(errorCode);
}

void AsyncClientDelegate::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    m_delegate->cancelHandlers(client, std::move(handler));
}

void AsyncClientDelegate::setKeepAliveOptions(
    nx::network::KeepAliveOptions options)
{
    m_delegate->setKeepAliveOptions(options);
}

void AsyncClientDelegate::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_delegate.reset();
}

nx::network::stun::AbstractAsyncClient& AsyncClientDelegate::delegate()
{
    return *m_delegate;
}

const nx::network::stun::AbstractAsyncClient& AsyncClientDelegate::delegate() const
{
    return *m_delegate;
}

} // namespace nx::network::stun
