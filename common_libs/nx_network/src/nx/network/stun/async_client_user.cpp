#include "async_client_user.h"

namespace nx {
namespace stun {

AsyncClientUser::~AsyncClientUser()
{
    QnMutexLocker lk( &m_mutex );
    NX_ASSERT(m_pleaseStopHasBeenCalled, Q_FUNC_INFO, "pleaseStop was not called");
}

SocketAddress AsyncClientUser::localAddress() const
{
    return m_client->localAddress();
}

SocketAddress AsyncClientUser::remoteAddress() const
{
    return m_client->remoteAddress();
}

AsyncClientUser::AsyncClientUser(std::shared_ptr<AbstractAsyncClient> client)
    : m_operationsInProgress(0)
    , m_client(std::move(client))
    , m_pleaseStopHasBeenCalled(false)
{
}

void AsyncClientUser::setOnReconnectedHandler(
    AbstractAsyncClient::ReconnectHandler handler)
{
    m_client->addOnReconnectedHandler(
        [self = shared_from_this(), handler = std::move(handler)]()
        {
            if (!self->startOperation())
                return;

            handler();
            self->stopOperation();
        });
}

void AsyncClientUser::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lk(&m_mutex);
    NX_ASSERT(!m_stopHandler, Q_FUNC_INFO,
              "pleaseStop(...) is called 2nd time before handler is waited");

    m_stopHandler = std::move(handler);
    m_pleaseStopHasBeenCalled = true;
    checkHandler(&lk);
}

void AsyncClientUser::sendRequest(
    Message request, AbstractAsyncClient::RequestHandler handler)
{
    m_client->sendRequest(
        std::move(request),
        [self = shared_from_this(), handler = std::move(handler)](
            SystemError::ErrorCode code, Message message) mutable
        {
            if (!self->startOperation())
                return;

            handler(code, std::move(message));
            self->stopOperation();
        });
}

bool AsyncClientUser::setIndicationHandler(
    int method, AbstractAsyncClient::IndicationHandler handler)
{
    return m_client->setIndicationHandler(
        method,
        [self = shared_from_this(), handler = std::move(handler)](
            Message message) mutable
        {
            if (!self->startOperation())
                return;

            handler(std::move(message));
            self->stopOperation();
        });
}

bool AsyncClientUser::startOperation()
{
    QnMutexLocker lk(&m_mutex);
    if (m_pleaseStopHasBeenCalled)
        return false;

    ++m_operationsInProgress;
    return true;
}

void AsyncClientUser::stopOperation()
{
    QnMutexLocker lk(&m_mutex);
    --m_operationsInProgress;
    checkHandler(&lk);
}

void AsyncClientUser::checkHandler(QnMutexLockerBase* lk)
{
    if (m_stopHandler && m_operationsInProgress == 0)
    {
        const auto client = std::move(m_client);
        m_client.reset();

        const auto handler = std::move(m_stopHandler);
        m_stopHandler = nullptr;

        lk->unlock();
        handler();
    }
}

} // namespase stun
} // namespase nx
