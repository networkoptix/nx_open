#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/utils/std/future.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class Relaying:
    public BasicTestFixture
{
    using base_type = BasicTestFixture;

private:
    virtual void SetUp() override
    {
        base_type::SetUp();

        // Disabling every method except relaying.
        //ConnectorFactory::setEnabledCloudConnectMask((int)CloudConnectType::proxy);

        startServer();
    }
};

TEST_F(Relaying, just_works)
{
    startExchangingFixedData();
    assertDataHasBeenExchangedCorrectly();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
