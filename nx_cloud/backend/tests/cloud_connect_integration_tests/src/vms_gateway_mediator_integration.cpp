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
    public BasicTestFixture
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
        mediator().setPreserveEndpointsDuringRestart(false);

        base_type::SetUp();

        auto serverSocket = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(serverSocket->setNonBlockingMode(true));
        ASSERT_TRUE(serverSocket->bind(SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(serverSocket->listen());

        m_mediatorTcpEndpoint = serverSocket->getLocalAddress();

        m_mediatorStunProxyId = m_streamProxy.addProxy(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(std::move(serverSocket)),
            mediator().stunTcpEndpoint());
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

        m_streamProxy.setProxyDestination(
            m_mediatorStunProxyId,
            mediator().stunTcpEndpoint());
    }

    void whenStartGateway()
    {
        // Specifying mediator port to gateway.
        const auto mediatorEndpointStr = m_mediatorTcpEndpoint.toStdString();
        m_vmsGateway.addArg("-general/mediatorEndpoint", mediatorEndpointStr.c_str());

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
    network::SocketAddress m_mediatorTcpEndpoint;
    nx::network::StreamProxy m_streamProxy;
    int m_mediatorStunProxyId = -1;
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
