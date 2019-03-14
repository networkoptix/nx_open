#include "stun_over_http_server.h"

namespace nx {
namespace network {
namespace stun {

const char* const StunOverHttpServer::kStunProtocolName = "STUN/rfc5389";

StunOverHttpServer::StunOverHttpServer(
    nx::network::stun::MessageDispatcher* stunMessageDispatcher)
    :
    m_dispatcher(stunMessageDispatcher),
    m_httpTunnelingServer(
        [this](auto connection) { createStunConnection(std::move(connection)); },
        nullptr)
{
}

StunOverHttpServer::StunConnectionPool& StunOverHttpServer::stunConnectionPool()
{
    return m_stunConnectionPool;
}

const StunOverHttpServer::StunConnectionPool& StunOverHttpServer::stunConnectionPool() const
{
    return m_stunConnectionPool;
}

void StunOverHttpServer::setInactivityTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_inactivityTimeout = timeout;
}

void StunOverHttpServer::createStunConnection(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    auto stunConnection = std::make_shared<nx::network::stun::ServerConnection>(
        std::move(connection),
        *m_dispatcher);
    m_stunConnectionPool.saveConnection(stunConnection);
    stunConnection->startReadingConnection(m_inactivityTimeout);
}

} // namespace stun
} // namespace network
} // namespace nx
