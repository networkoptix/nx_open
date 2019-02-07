#pragma once

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>

#include "api/relay_api_client_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

class BasicFixture:
    public ::testing::Test
{
public:
    BasicFixture();
    ~BasicFixture();

protected:
    virtual void onClientToRelayConnectionInstanciated(
        nx::cloud::relay::api::test::ClientStub*) {}
    virtual void onClientToRelayConnectionDestroyed(
        nx::cloud::relay::api::test::ClientStub*) {}

    void resetClientFactoryToDefault();

private:
    boost::optional<nx::cloud::relay::api::ClientFactory::Function>
        m_clientFactoryBak;

    std::unique_ptr<nx::cloud::relay::api::AbstractClient>
        clientFactoryFunc(const utils::Url & /*relayUrl*/);
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
