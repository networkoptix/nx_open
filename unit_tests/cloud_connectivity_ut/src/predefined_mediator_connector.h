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
        const SocketAddress& udpEndpoint,
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> serverConnection);

    virtual std::unique_ptr<hpm::api::MediatorClientTcpConnection> clientConnection() override;
    virtual std::unique_ptr<hpm::api::MediatorServerTcpConnection> systemConnection() override;
    virtual boost::optional<SocketAddress> udpEndpoint() const override;
    virtual void setOnMediatorAvailabilityChanged(
        hpm::api::MediatorAvailabilityChangedHandler handler) override;

private:
    SocketAddress m_udpEndpoint;
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_serverConnection;
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
