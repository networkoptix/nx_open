#include "predefined_mediator_connector.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

PredefinedMediatorConnector::PredefinedMediatorConnector(
    const hpm::api::MediatorAddress& mediatorAddress,
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> serverConnection)
    :
    m_mediatorAddress(mediatorAddress),
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

void PredefinedMediatorConnector::fetchAddress(
    nx::utils::MoveOnlyFunc<void(
        http::StatusCode::Value /*resultCode*/,
        hpm::api::MediatorAddress /*address*/)> handler)
{
    post(
        [this, handler = std::move(handler)]() 
        {
            handler(http::StatusCode::ok, m_mediatorAddress);
        });
}

std::optional<hpm::api::MediatorAddress> PredefinedMediatorConnector::address() const
{
    return m_mediatorAddress;
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
