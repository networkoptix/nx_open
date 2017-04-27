#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>

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
    nx::cloud::relay::api::ClientFactory::CustomFactoryFunc
        m_clientFactoryBak;

    std::unique_ptr<nx::cloud::relay::api::Client> 
        clientFactoryFunc(const QUrl& /*relayUrl*/);
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
