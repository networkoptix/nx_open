#include "abstract_tunnel_validator.h"

namespace nx::network::http::tunneling {

AbstractTunnelValidator::AbstractTunnelValidator(
    std::unique_ptr<AbstractStreamSocket> connection)
    :
    m_connection(std::move(connection))
{
    bindToAioThread(m_connection->getAioThread());
}

void AbstractTunnelValidator::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

std::unique_ptr<AbstractStreamSocket> AbstractTunnelValidator::takeConnection()
{
    return std::exchange(m_connection, nullptr);
}

void AbstractTunnelValidator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
}

} // namespace nx::network::http::tunneling
