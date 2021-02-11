#include "stream_socket_connector.h"

#include "../socket_factory.h"

namespace nx::network::aio {

void StreamSocketConnector::bindToAioThread(AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_streamSocket)
        m_streamSocket->bindToAioThread(aioThread);
}

void StreamSocketConnector::connectAsync(
    const network::SocketAddress& targetEndpoint,
    ConnectHandler handler)
{
    m_streamSocket = SocketFactory::createStreamSocket();
    m_streamSocket->setNonBlockingMode(true);
    m_streamSocket->bindToAioThread(getAioThread());
    m_streamSocket->connectAsync(
        targetEndpoint,
        [this, handler = std::move(handler)](auto result)
        {
            handler(result, std::exchange(m_streamSocket, nullptr));
        });
}

void StreamSocketConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_streamSocket.reset();
}

} // namespace nx::network::aio
