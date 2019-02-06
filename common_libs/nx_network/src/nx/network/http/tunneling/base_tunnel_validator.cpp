#include "base_tunnel_validator.h"

namespace nx_http::tunneling {

BaseTunnelValidator::BaseTunnelValidator(
    std::unique_ptr<AbstractStreamSocket> connection)
    :
    m_connection(std::move(connection))
{
    bindToAioThread(m_connection->getAioThread());
}

void BaseTunnelValidator::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

std::unique_ptr<AbstractStreamSocket> BaseTunnelValidator::takeConnection()
{
    return std::exchange(m_connection, nullptr);
}

void BaseTunnelValidator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
}

} // namespace nx_http::tunneling
