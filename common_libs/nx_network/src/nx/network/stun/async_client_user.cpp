#include "async_client_user.h"

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
}

void AsyncClientUser::setOnReconnectedHandler(
    AbstractAsyncClient::ReconnectHandler handler)
{
    m_client->addOnReconnectedHandler(std::move(handler), this);
}

void AsyncClientUser::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_client->cancelHandlers(this, std::move(handler));
}

void AsyncClientUser::sendRequest(
    Message request, AbstractAsyncClient::RequestHandler handler)
{
    m_client->sendRequest(std::move(request), std::move(handler), this);
}

bool AsyncClientUser::setIndicationHandler(
    int method, AbstractAsyncClient::IndicationHandler handler)
{
    return m_client->setIndicationHandler(method, std::move(handler), this);
}

} // namespase stun
} // namespase nx
