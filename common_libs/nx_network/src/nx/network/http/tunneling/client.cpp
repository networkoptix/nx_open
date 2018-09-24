#include "client.h"

#include "detail/client_factory.h"

namespace nx_http::tunneling {

Client::Client(const QUrl& baseTunnelUrl):
    m_actualClient(detail::ClientFactory::instance().create(baseTunnelUrl))
{
    m_actualClient->bindToAioThread(getAioThread());
}

void Client::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_actualClient)
        m_actualClient->bindToAioThread(aioThread);
}

void Client::setTimeout(std::chrono::milliseconds timeout)
{
    m_actualClient->setTimeout(timeout);
}

void Client::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    m_actualClient->openTunnel(std::move(completionHandler));
}

const Response& Client::response() const
{
    return m_actualClient->response();
}

void Client::stopWhileInAioThread()
{
    m_actualClient.reset();
}

} // namespace nx_http::tunneling
