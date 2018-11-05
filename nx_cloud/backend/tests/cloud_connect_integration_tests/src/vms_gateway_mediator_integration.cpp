#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>

#include <libvms_gateway_core/src/test_support/vms_gateway_functional_test.h>

#include <controller.h>
#include <mediator_service.h>
#include <peer_registrator.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class VmsGatewayMediatorIntegration:
    public BasicTestFixture,
    public ::testing::Test
{
    using base_type = BasicTestFixture;

public:
    VmsGatewayMediatorIntegration():
        m_vmsGateway(nx::cloud::gateway::VmsGatewayFunctionalTest::doNotReinitialiseSocketGlobals),
        m_vmsGatewayPeerId(SocketGlobals::cloud().outgoingTunnelPool().ownPeerId().toStdString())
    {
        setInitFlags(BasicTestFixture::doNotInitializeMediatorConnection);
    }

protected:
    virtual void SetUp() override
    {
        // NOTE: BasicTestFixture does not inherit ::testing::Test anymore.
        // So, base_type::SetUp() is not called automatically.
        base_type::SetUp();
    }

    void givenGatewayThatFailedToConnectToMediator()
    {
        mediator().stop();

        whenStartGateway();

        // Waiting for gateway to fail request.
        // TODO: #ak Replace with an event.
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    void whenStartMediator()
    {
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());
    }

    void whenStartGateway()
    {
        // Specifying mediator port to gateway.
        const auto mediatorEndpointStr = mediator().stunTcpEndpoint().toStdString();
        m_vmsGateway.addArg("-general/mediatorEndpoint", mediatorEndpointStr.c_str());
        m_vmsGateway.addArg("--cloudConnect/publicIpAddress=127.0.0.1");

        ASSERT_TRUE(m_vmsGateway.startAndWaitUntilStarted());
    }

    void thenGatewayReconnectedToMediator()
    {
        // TODO: #ak Probably, it is better to check that gateway
        // endpoint is in mediator connection_requested event.

        for (;;)
        {
            auto listeningPeers =
                mediator().moduleInstance()->impl()->controller()
                    .listeningPeerRegistrator().getListeningPeers().clients;
            for (const auto& listeningPeer: listeningPeers)
            {
                if (listeningPeer.first.toStdString() == m_vmsGatewayPeerId)
                {
                    //ASSERT_EQ(listeningPeer.second.);
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    nx::cloud::gateway::VmsGatewayFunctionalTest m_vmsGateway;
    const std::string m_vmsGatewayPeerId;
};

TEST_F(VmsGatewayMediatorIntegration, gateway_reconnects_to_mediator)
{
    givenGatewayThatFailedToConnectToMediator();
    whenStartMediator();
    thenGatewayReconnectedToMediator();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
