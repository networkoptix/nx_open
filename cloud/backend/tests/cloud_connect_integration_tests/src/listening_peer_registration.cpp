#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>

#include "basic_test_fixture.h"

namespace nx::network::cloud::test {

class ListenigPeerRegistration:
    public BasicTestFixture,
    public ::testing::Test
{
    using base_type = BasicTestFixture;

protected:
    virtual void SetUp() override
    {
        configureProxyBeforeMediator();
        mediator().addArg("-stun/keepAliveOptions", "{1, 1, 2}"); //< 1 second probe period.

        setRelayCount(0);

        base_type::SetUp();
    }

    void givenListeningCloudServer()
    {
        startServer();
        waitUntilServerIsRegisteredOnMediator();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void whenMediatorConnectionIsDroppedSilently()
    {
        blockDownTrafficThroughExistingConnectionsToMediator();
    }

    void thenConnectionToMediatorIsRestored()
    {
        waitUntilServerIsRegisteredOnMediator();
    }
};

TEST_F(ListenigPeerRegistration, reconnects_after_mediator_restart)
{
    givenListeningCloudServer();
    restartMediator();
    thenConnectionToMediatorIsRestored();
}

TEST_F(ListenigPeerRegistration, reconnects_after_keep_alive_failure)
{
    givenListeningCloudServer();

    for (int i = 0; i < 2; ++i)
    {
        nx::network::SocketGlobals::instance().cloud().outgoingTunnelPool().removeAllTunnelsSync();

        whenMediatorConnectionIsDroppedSilently();
        waitUntilCloudConnectionCanBeEstablished();
    }
}

} // namespace nx::network::cloud::test
