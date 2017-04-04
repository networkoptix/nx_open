#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/client_to_relay_connection.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

class ClientToRelayConnection;

class CloudRelayFixtureBase:
    public ::testing::Test
{
public:
    CloudRelayFixtureBase();
    ~CloudRelayFixtureBase();

protected:
    virtual void onClientToRelayConnectionInstanciated(ClientToRelayConnection*) {}
    virtual void onClientToRelayConnectionDestroyed(ClientToRelayConnection*) {}

private:
    api::ClientToRelayConnectionFactory::CustomFactoryFunc m_clientToRelayConnectionFactoryBak;

    std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnectionFactoryFunc(
        const SocketAddress& /*relayEndpoint*/);
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
