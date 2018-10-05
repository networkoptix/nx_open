#include "predefined_mediator_connector.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

PredefinedMediatorConnector::PredefinedMediatorConnector(
    const nx::network::SocketAddress& udpEndpoint,
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> serverConnection)
    :
    m_udpEndpoint(udpEndpoint),
    m_serverConnection(std::move(serverConnection))
{
}

std::unique_ptr<hpm::api::MediatorClientTcpConnection>
    PredefinedMediatorConnector::clientConnection()
{
    return nullptr;
}

std::unique_ptr<hpm::api::MediatorServerTcpConnection>
    PredefinedMediatorConnector::systemConnection()
{
    return std::move(m_serverConnection);
}

void PredefinedMediatorConnector::fetchUdpEndpoint(
    nx::utils::MoveOnlyFunc<void(
        http::StatusCode::Value /*resultCode*/,
        SocketAddress /*endpoint*/)> handler)
{
    post(
        [this, handler = std::move(handler)]() 
        {
            handler(http::StatusCode::ok, m_udpEndpoint);
        });
}

std::optional<nx::network::SocketAddress> PredefinedMediatorConnector::udpEndpoint() const
{
    return m_udpEndpoint;
}

void PredefinedMediatorConnector::setOnMediatorAvailabilityChanged(
    hpm::api::MediatorAvailabilityChangedHandler /*handler*/)
{
    // TODO
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
