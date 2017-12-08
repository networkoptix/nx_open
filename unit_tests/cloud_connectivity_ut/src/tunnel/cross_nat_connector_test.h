#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/connector_factory.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class TunnelConnector:
    public ::testing::Test
{
public:
    ~TunnelConnector();

    void setConnectorFactoryFunc(
        CrossNatConnectorFactory::Function newFactoryFunc);
    const hpm::MediatorFunctionalTest& mediator() const;
    hpm::MediatorFunctionalTest& mediator();

protected:
    struct ConnectResult
    {
        SystemError::ErrorCode errorCode = SystemError::noError;
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection;
        std::chrono::milliseconds executionTime = std::chrono::milliseconds::zero();
    };

    ConnectResult doSimpleConnectTest(
        std::chrono::milliseconds connectTimeout,
        nx::hpm::MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
        boost::optional<SocketAddress> mediatorAddressForConnector = boost::none,
        std::function<void(nx::hpm::MediaServerEmulator*)> serverConfig = nullptr);
    ConnectResult doSimpleConnectTest(
        std::chrono::milliseconds connectTimeout,
        nx::hpm::MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
        const nx::hpm::AbstractCloudDataProvider::System& system,
        const std::unique_ptr<nx::hpm::MediaServerEmulator>& server,
        boost::optional<SocketAddress> mediatorAddressForConnector = boost::none);

    void generalTest();
    void cancellationTest();

    void doSimpleConnectTest(
        std::chrono::milliseconds connectTimeout,
        nx::hpm::MediaServerEmulator::ActionToTake actionOnConnectAckResponse,
        const nx::hpm::AbstractCloudDataProvider::System& system,
        const std::unique_ptr<nx::hpm::MediaServerEmulator>& server,
        boost::optional<SocketAddress> mediatorAddressForConnector,
        ConnectResult* const connectResult);

private:
    boost::optional<CrossNatConnectorFactory::Function> m_oldFactoryFunc;
    nx::hpm::MediatorFunctionalTest m_mediator;
};

} //namespace test
} //namespace cloud
} //namespace network
} //namespace nx
