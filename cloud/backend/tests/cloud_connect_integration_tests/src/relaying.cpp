#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/sync_call.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

struct RelayingConfiguration
{
    const bool useHttpConnect;
    const bool relayAcceptsSslOnly;

    RelayingConfiguration() = delete;
};

static constexpr RelayingConfiguration kUsingHttpConnect{true, false};
static constexpr RelayingConfiguration kNotUsingHttpConnect{false, false};
static constexpr RelayingConfiguration kRelayAcceptsSslOnly{false, true};

class Relaying:
    public BasicTestFixture,
    public ::testing::TestWithParam<RelayingConfiguration>
{
    using base_type = BasicTestFixture;

public:
    Relaying()
    {
        if (GetParam().relayAcceptsSslOnly)
        {
            addRelayStartupArgument("http/listenOn", "");
            addRelayStartupArgument("https/listenOn", "127.0.0.1:0");
        }
    }

    ~Relaying()
    {
        if (m_useHttpConnectToListenOnRelayBak)
        {
            SocketGlobals::cloud().settings().useHttpConnectToListenOnRelay =
               *m_useHttpConnectToListenOnRelayBak;
        }
    }

protected:
    void startMoreConnectionsThatAreFoundOnRelay()
    {
        for (int i = 0; i < 101; ++i)
            startExchangingFixedData();
    }

    void assertServerIsListeningOnRelay()
    {
        using namespace nx::cloud::relay;

        auto relayClient = std::make_unique<api::Client>(relayUrl());

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
    std::optional<bool> m_useHttpConnectToListenOnRelayBak;

    virtual void SetUp() override
    {
        base_type::SetUp();

        // Disabling every method except relaying.
        ConnectorFactory::setEnabledCloudConnectMask((int)ConnectType::proxy);

        m_useHttpConnectToListenOnRelayBak =
            SocketGlobals::cloud().settings().useHttpConnectToListenOnRelay;
        SocketGlobals::cloud().settings().useHttpConnectToListenOnRelay =
            GetParam().useHttpConnect;

        startServer();
    }
};

TYPED_TEST_CASE_P(Relaying);

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_P(Relaying, server_socket_registers_itself_on_relay)
{
    this->assertServerIsListeningOnRelay();
}

TEST_P(Relaying, connection_can_be_established)
{
    this->assertConnectionCanBeEstablished();
}

TEST_P(Relaying, connecting_using_full_server_name)
{
    this->setRemotePeerName(this->cloudSystemCredentials().hostName().toStdString());
    this->assertConnectionCanBeEstablished();
}

TEST_P(Relaying, exchanging_fixed_data)
{
    this->startExchangingFixedData();
    this->assertDataHasBeenExchangedCorrectly();
}

TEST_P(Relaying, multiple_connections)
{
    this->startMoreConnectionsThatAreFoundOnRelay();
    this->assertDataHasBeenExchangedCorrectly();
}

INSTANTIATE_TEST_CASE_P(
    UsingHttpConnectRelaying,
    Relaying,
    ::testing::Values(kUsingHttpConnect));

INSTANTIATE_TEST_CASE_P(
    NotUsingHttpConnect,
    Relaying,
    ::testing::Values(kNotUsingHttpConnect));

INSTANTIATE_TEST_CASE_P(
    SslOnly,
    Relaying,
    ::testing::Values(kRelayAcceptsSslOnly));

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
