#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/test_support/test_options.h>

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
        nx::network::SocketGlobalsHolder::instance()->reinitialize();

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

    std::unique_ptr<AbstractStreamServerSocket> cloudServerSocket()
    {
        auto serverSocket = std::make_unique<CloudServerSocket>(
            SocketGlobals::mediatorConnector().systemConnection());

        serverSocket->setSupportedConnectionMethods(hpm::api::ConnectionMethod::udpHolePunching);
        NX_CRITICAL(serverSocket->registerOnMediatorSync() == hpm::api::ResultCode::ok);
        return std::unique_ptr<AbstractStreamServerSocket>(std::move(serverSocket));
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
        m_server = mediator().addRandomServer(
            m_system, boost::none, hpm::ServerTweak::noBindEndpoint);

        ASSERT_NE(nullptr, m_server);
        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                m_system.id,
                m_server->serverId(),
                m_system.authKey));

        SocketGlobals::mediatorConnector().mockupAddress(mediator().stunEndpoint());
        SocketGlobals::mediatorConnector().enable(true);
    }
};

NX_NETWORK_TRANSMIT_SOCKET_TESTS_CASE_EX(
    TEST_F, UdpHolePunching,
    [&](){ return cloudServerSocket(); },
    [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
    SocketAddress(m_server->fullName()));

TEST_F(UdpHolePunching, SimpleSyncSsl)
{
    network::test::socketSimpleSync(
        [&]() { return std::make_unique<SslServerSocket>(cloudServerSocket().release(), false); },
        []() { return std::make_unique<SslSocket>(new CloudStreamSocket(AF_INET), false); },
        SocketAddress(m_server->fullName()));
}

TEST_F(UdpHolePunching, loadTest)
{
    const std::chrono::seconds testDuration(7 * utils::TestOptions::timeoutMultiplier());
    const int maxSimultaneousConnections = 25;
    const int bytesToSendThroughConnection = 1024 * 1024;

    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none,
        bytesToSendThroughConnection,
        test::TestTransmissionMode::spam);

    server.setServerSocket(cloudServerSocket());
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
