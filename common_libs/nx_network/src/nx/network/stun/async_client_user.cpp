#include "async_client_user.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace stun {

SocketAddress AsyncClientUser::localAddress() const
{
    return m_client->localAddress();
}

SocketAddress AsyncClientUser::remoteAddress() const
{
    return m_client->remoteAddress();
}

AsyncClientUser::AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client)
:
    m_client(std::move(client))
{
    NX_LOGX("ready", cl_logDEBUG2);
}

void AsyncClientUser::setOnReconnectedHandler(
    AbstractAsyncClient::ReconnectHandler handler)
{
    m_client->addOnReconnectedHandler(
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)]()
        {
            if (auto lock = guard->lock())
                return post(std::move(handler));

            NX_LOGX(lm("ignore reconnect handler"), cl_logDEBUG2);
        },
        m_asyncGuard.sharedGuard().get());
}

void AsyncClientUser::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_asyncGuard.reset();
    network::aio::Timer::pleaseStop(
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)]()
        {
            if (m_client)
            {
                m_client->cancelHandlers(
                    guard.get(),
                    [guard = std::move(guard)]() mutable
                    {
                        // guard shell be kept here up to the end of cancelation
                        // to prevent reuse of the same address
                    });

                m_client.reset();
                NX_LOGX("stopped", cl_logDEBUG2);
            }

            // we're good to go here regardless of cancelHandlers(...) progress
            handler();
        });
}

void AsyncClientUser::pleaseStopSync()
{
    QnStoppableAsync::pleaseStopSync();
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
                return post(std::bind(std::move(handler), code, std::move(message)));

            NX_LOGX(lm("ignore response %1 handler")
                .arg(message.header.transactionId.toHex()), cl_logDEBUG2);
        },
        m_asyncGuard.sharedGuard().get());
}

bool AsyncClientUser::setIndicationHandler(
    int method, AbstractAsyncClient::IndicationHandler handler)
{
    return m_client->setIndicationHandler(
        method,
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)](
            Message message)
        {
            if (auto lock = guard->lock())
                return post(std::bind(std::move(handler), std::move(message)));

            NX_LOGX(lm("ignore indication %1 handler")
                .arg(message.header.method), cl_logDEBUG2);
        },
        m_asyncGuard.sharedGuard().get());
}

} // namespase stun
} // namespase nx
