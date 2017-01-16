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
    NX_LOGX("Ready", cl_logDEBUG2);
}

AsyncClientUser::~AsyncClientUser()
{
    // Just in case it's called from own AIO thread without explicit pleaseStop.
    disconnectFromClient();
}

void AsyncClientUser::setOnReconnectedHandler(
    AbstractAsyncClient::ReconnectHandler handler)
{
    m_client->addOnReconnectedHandler(
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)]()
        {
            if (auto lock = guard->lock())
                return post(std::move(handler));

            NX_LOG(lm("AsyncClientUser(%1). Ignoring reconnect handler")
                .arg(this), cl_logDEBUG1);
        },
        m_asyncGuard.sharedGuard().get());
}

void AsyncClientUser::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    disconnectFromClient();
    network::aio::Timer::pleaseStop(std::move(handler));
}

void AsyncClientUser::pleaseStopSync(bool checkForLocks)
{
    disconnectFromClient();
    network::aio::Timer::pleaseStopSync(checkForLocks);
}

AbstractAsyncClient* AsyncClientUser::client() const
{
    return m_client.get();
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
                return post(
                    [handler = std::move(handler), code, message = std::move(message)]() mutable
                    {
                        handler(code, std::move(message));
                    });

            NX_LOG(lm("AsyncClientUser(%1). Ignore response %2 handler")
                .arg(this).arg(message.header.transactionId.toHex()), cl_logDEBUG1);
        },
        m_asyncGuard.sharedGuard().get());
}

bool AsyncClientUser::setIndicationHandler(
    int method, AbstractAsyncClient::IndicationHandler handler)
{
    return m_client->setIndicationHandler(
        method,
        [this, guard = m_asyncGuard.sharedGuard(), handler = std::move(handler)](Message message)
        {
            if (auto lock = guard->lock())
                return post(std::bind(std::move(handler), std::move(message)));

            NX_LOG(lm("AsyncClientUser(%1). Ignore indication %2 handler")
                .arg(this).arg(message.header.method), cl_logDEBUG1);
        },
        m_asyncGuard.sharedGuard().get());
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

    m_asyncGuard.reset();
    NX_LOG(lm("AsyncClientUser(%1). Disconnected from client").arg(this), cl_logDEBUG2);
}

} // namespace stun
} // namespace nx
