#include "async_client_user.h"

namespace nx {
namespace stun {

AsyncClientUser::~AsyncClientUser()
{
    QnMutexLocker lk( &m_mutex );
    Q_ASSERT_X(m_stopHandler, Q_FUNC_INFO, "pleaseStop was not called");
}

SocketAddress AsyncClientUser::localAddress() const
{
    return m_client->localAddress();
}

AsyncClientUser::AsyncClientUser(std::shared_ptr<AsyncClient> client)
    : m_operationsInProgress(0)
    , m_client(std::move(client))
{
}

void AsyncClientUser::pleaseStop(std::function<void()> handler)
{
    QnMutexLocker lk(&m_mutex);
    Q_ASSERT_X(!m_stopHandler, Q_FUNC_INFO, "pleaseStop is called 2nd time");
    m_stopHandler = std::move(handler);
    m_client.reset();
    checkHandler(&lk);
}

void AsyncClientUser::sendRequest(Message request,
                                  AsyncClient::RequestHandler handler)
{


    auto wrapper = [this](const std::shared_ptr<AsyncClientUser>& self,
                      const AsyncClient::RequestHandler& handler,
                      SystemError::ErrorCode code, Message message)
    {
        if (!self->startOperation())
            return;

        handler(code, std::move(message));
        self->stopOperation();
    };

    using namespace std::placeholders;
    m_client->sendRequest(std::move(request), std::bind(
        std::move(wrapper), shared_from_this(), std::move(handler), _1, _2));
}

bool AsyncClientUser::setIndicationHandler(int method,
                                         AsyncClient::IndicationHandler handler)
{
    auto wrapper = [method](const std::shared_ptr<AsyncClientUser>& self,
                            const AsyncClient::IndicationHandler& handler,
                            Message message)
    {
        if (!self->startOperation())
        {
            self->m_client->ignoreIndications(method); // break the cycle
            return;
        }

        handler(std::move(message));
        self->stopOperation();
    };

    using namespace std::placeholders;
    return m_client->setIndicationHandler(
        method,
        std::bind(
            std::move(wrapper), shared_from_this(), std::move(handler), _1));
}

bool AsyncClientUser::startOperation()
{
    QnMutexLocker lk(&m_mutex);
    if (m_stopHandler)
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
        const auto handler = m_stopHandler;

        lk->unlock();
        handler();
    }
}

} // namespase stun
} // namespase nx
