#include "basic_test_fixture.h"

namespace nx::network::cloud::test {

class ConnectivityWithoutUdp:
    public BasicTestFixture,
    public ::testing::Test
{
    using base_type = BasicTestFixture;

protected:
    virtual void SetUp() override
    {
        // Disabling UDP on mediator.

        mediator().removeArgByName("stun/udpAddrToListenList");
        mediator().addArg("--stun/udpAddrToListenList=");

        base_type::SetUp();

        startServer();
    }
};

TEST_F(ConnectivityWithoutUdp, connect_works)
{
    assertConnectionCanBeEstablished();
}

} // namespace nx::network::cloud::test
