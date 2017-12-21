#include <thread>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/sync_call.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class Relaying:
    public BasicTestFixture
{
    using base_type = BasicTestFixture;

protected:
    void startMoreConnectionsThatAreFoundOnRelay()
    {
        for (int i = 0; i < 101; ++i)
            startExchangingFixedData();
    }

    void assertServerIsListeningOnRelay()
    {
        using namespace nx::cloud::relay;

        auto relayClient = api::ClientFactory::create(relayUrl());

        for (;;)
        {
            std::promise<
                std::tuple<api::ResultCode, api::CreateClientSessionResponse>
            > requestCompletion;

            relayClient->startSession(
                "",
                serverSocketCloudAddress(),
                [this, &requestCompletion](
                    api::ResultCode resultCode,
                    api::CreateClientSessionResponse response)
                {
                    requestCompletion.set_value(std::make_tuple(resultCode, std::move(response)));
                });

            const auto result = requestCompletion.get_future().get();
            if (std::get<0>(result) == api::ResultCode::ok)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    virtual void SetUp() override
    {
        base_type::SetUp();

        // Disabling every method except relaying.
        ConnectorFactory::setEnabledCloudConnectMask((int)ConnectType::proxy);

        startServer();
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(Relaying, server_socket_registers_itself_on_relay)
{
    assertServerIsListeningOnRelay();
}

TEST_F(Relaying, connection_can_be_established)
{
    assertConnectionCanBeEstablished();
}

TEST_F(Relaying, connecting_using_full_server_name)
{
    setRemotePeerName(cloudSystemCredentials().hostName());
    assertConnectionCanBeEstablished();
}

TEST_F(Relaying, exchanging_fixed_data)
{
    startExchangingFixedData();
    assertDataHasBeenExchangedCorrectly();
}

TEST_F(Relaying, multiple_connections)
{
    startMoreConnectionsThatAreFoundOnRelay();
    assertDataHasBeenExchangedCorrectly();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
