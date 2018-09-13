#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tunnel_acceptor_factory.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class CloudConnectOptionsServerSide:
    public BasicTestFixture
{
    using base_type = BasicTestFixture;

protected:
    void whenDisableUdpHolePunching()
    {
        TunnelAcceptorFactory::instance().setUdpHolePunchingEnabled(false);
    }

    void thenConnectionCanBeEstablished()
    {
        assertConnectionCanBeEstablished();
    }

private:
    virtual void SetUp() override
    {
        base_type::SetUp();

        startServer();
    }
};

TEST_F(CloudConnectOptionsServerSide, disabling_udp_hole_punching)
{
    whenDisableUdpHolePunching();
    thenConnectionCanBeEstablished();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
