#pragma once

#include <nx/network/cloud/mediator_connector.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class PredefinedMediatorConnector:
    public hpm::api::AbstractMediatorConnector
{
public:
    PredefinedMediatorConnector(
        const hpm::api::MediatorAddress& mediatorAddress,
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> serverConnection);

    virtual std::unique_ptr<hpm::api::MediatorClientTcpConnection> clientConnection() override;

    virtual std::unique_ptr<hpm::api::MediatorServerTcpConnection> systemConnection() override;

    virtual void fetchAddress(nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        hpm::api::MediatorAddress /*address*/)> handler) override;

    virtual std::optional<hpm::api::MediatorAddress> address() const override;

private:
    hpm::api::MediatorAddress m_mediatorAddress;
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_serverConnection;
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
