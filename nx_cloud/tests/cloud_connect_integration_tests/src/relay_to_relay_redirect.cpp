#include <gtest/gtest.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class RelayToRelayRedirect: public BasicTestFixture
{
public:
    RelayToRelayRedirect():
        BasicTestFixture(3)
    {}

    virtual void SetUp() override
    {
        BasicTestFixture::SetUp();

        ConnectorFactory::setEnabledCloudConnectMask((int)CloudConnectType::proxy);

        startServer();
    }

    void assertServerIsAccessibleFromAnotherRelay()
    {
        using namespace nx::cloud::relay;

        auto relayClient = api::ClientFactory::create(relayUrl(2));

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
            qWarning() << "waiting...";
        }
    }
};

TEST_F(RelayToRelayRedirect, RedirectToAnotherRelay)
{
    assertServerIsAccessibleFromAnotherRelay();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx