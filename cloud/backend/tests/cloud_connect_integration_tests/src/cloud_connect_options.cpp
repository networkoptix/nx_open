#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tunnel_acceptor_factory.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class CloudConnectOptionsServerSide:
    public BasicTestFixture,
    public ::testing::Test
{
    using base_type = BasicTestFixture;

public:
    CloudConnectOptionsServerSide():
        m_originalConnectionMethods(TunnelAcceptorFactory::instance().enabledConnectionMethods())
    {
    }

    ~CloudConnectOptionsServerSide()
    {
        TunnelAcceptorFactory::instance().setEnabledConnectionMethods(
            m_originalConnectionMethods);
    }

protected:
    void whenDisableUdpHolePunching()
    {
        TunnelAcceptorFactory::instance().setUdpHolePunchingEnabled(false);
    }

    void thenConnectionCanBeEstablished()
    {
        assertCloudConnectionCanBeEstablished();
    }

private:
    const hpm::api::ConnectionMethods m_originalConnectionMethods;

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
