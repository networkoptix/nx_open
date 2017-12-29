#include "stun_over_http_server.h"

namespace nx {
namespace network {
namespace stun {

const char* const StunOverHttpServer::kStunProtocolName = "STUN/rfc5389";

StunOverHttpServer::StunOverHttpServer(
    nx::network::stun::MessageDispatcher* stunMessageDispatcher)
    :
    m_dispatcher(stunMessageDispatcher)
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

void StunOverHttpServer::createStunConnection(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    auto stunConnection = std::make_shared<nx::network::stun::ServerConnection>(
        &m_stunConnectionPool,
        std::move(connection),
        *m_dispatcher);
    m_stunConnectionPool.saveConnection(stunConnection);
    stunConnection->startReadingConnection();
}

} // namespace stun
} // namespace network
} // namespace nx
