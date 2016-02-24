/**********************************************************
* Feb 24, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <libconnection_mediator/src/test_support/socket_globals_holder.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/simple_socket_test_helper.h>


namespace nx {
namespace network {
namespace cloud {

using nx::hpm::MediaServerEmulator;

class UdpHolePunching
:
    public ::testing::Test
{
public:
    UdpHolePunching()
    {
        SocketGlobalsHolder::instance()->reinitialize();
    }

    const hpm::MediatorFunctionalTest& mediator() const
    {
        return m_mediator;
    }

    hpm::MediatorFunctionalTest& mediator()
    {
        return m_mediator;
    }

private:
    hpm::MediatorFunctionalTest m_mediator;
};

//NX_NETWORK_BOTH_SOCKETS_TEST_CASE(TEST_F, UdpHolePunching, )

TEST_F(UdpHolePunching, general)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    //registering server in mediator
    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServerNotRegisteredOnMediator(system1);
    ASSERT_NE(nullptr, server1);

    SocketGlobals::mediatorConnector().setSystemCredentials(
        nx::hpm::api::SystemCredentials(
            system1.id,
            server1->serverId(),
            system1.authKey));
    SocketGlobals::mediatorConnector().mockupAddress(mediator().endpoint());
    SocketGlobals::mediatorConnector().enable(true);

    test::socketSimpleSync(
        []{
            return std::make_unique<CloudServerSocket>(
                SocketGlobals::mediatorConnector().systemConnection());
        },
        &std::make_unique<CloudStreamSocket>,
        SocketAddress(HostAddress::localhost, 0),
        SocketAddress(server1->fullName()));

    //socketSimpleAsync(
    //    &std::make_unique<CloudServerSocket>,
    //    &std::make_unique<CloudStreamSocket>,
    //    SocketAddress(HostAddress::localhost, 0),
    //    SocketAddress(server1->fullName()));
}

}   //cloud
}   //network
}   //nx
