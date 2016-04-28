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
#include <nx/network/test_support/socket_test_helper.h>


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

        init();
    }

    const hpm::MediatorFunctionalTest& mediator() const
    {
        return m_mediator;
    }

    hpm::MediatorFunctionalTest& mediator()
    {
        return m_mediator;
    }

protected:
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_server;

private:
    hpm::MediatorFunctionalTest m_mediator;

    void init()
    {
        //starting mediator
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());

        //registering server in mediator
        m_system = mediator().addRandomSystem();
        m_server = mediator().addRandomServerNotRegisteredOnMediator(m_system);
        ASSERT_NE(nullptr, m_server);

        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                m_system.id,
                m_server->serverId(),
                m_system.authKey));
        SocketGlobals::mediatorConnector().mockupAddress(mediator().endpoint());
        SocketGlobals::mediatorConnector().enable(true);
    }
};

TEST_F(UdpHolePunching, simpleSync)
{
    test::socketSimpleSync(
        []{
            auto serverSocket = std::make_unique<CloudServerSocket>(
                SocketGlobals::mediatorConnector().systemConnection());
            NX_CRITICAL(serverSocket->registerOnMediatorSync());
            return serverSocket;
        },
        &std::make_unique<CloudStreamSocket>,
        SocketAddress(HostAddress::localhost, 0),
        SocketAddress(m_server->fullName()));
}

TEST_F(UdpHolePunching, simpleAsync)
{
    test::socketSimpleAsync(
        []{
            auto serverSocket = std::make_unique<CloudServerSocket>(
                SocketGlobals::mediatorConnector().systemConnection());
            NX_CRITICAL(serverSocket->registerOnMediatorSync());
            return serverSocket;
        },
        &std::make_unique<CloudStreamSocket>,
        SocketAddress(HostAddress::localhost, 0),
        SocketAddress(m_server->fullName()));
}

TEST_F(UdpHolePunching, loadTest)
{
    const std::chrono::seconds testDuration(7);
    const int maxSimultaneousConnections = 25;
    const int bytesToSendThroughConnection = 1024 * 1024;

    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none,
        bytesToSendThroughConnection,
        test::TestTransmissionMode::spam);
    auto serverSocket = std::make_unique<CloudServerSocket>(
        SocketGlobals::mediatorConnector().systemConnection());
    ASSERT_TRUE(serverSocket->registerOnMediatorSync());
    server.setServerSocket(std::move(serverSocket));
    ASSERT_TRUE(server.start());

    test::ConnectionsGenerator connectionsGenerator(
        SocketAddress(QString::fromUtf8(m_server->fullName()), 0),
        maxSimultaneousConnections,
        test::TestTrafficLimitType::incoming,
        bytesToSendThroughConnection,
        test::ConnectionsGenerator::kInfiniteConnectionCount,
        test::TestTransmissionMode::spam);
    connectionsGenerator.start();

    std::this_thread::sleep_for(testDuration);

    connectionsGenerator.pleaseStopSync();

    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0);
    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0);

    server.pleaseStopSync();
}

}   //cloud
}   //network
}   //nx
