#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class TcpReverseConnectTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();
        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());

        m_system = m_mediator.addRandomSystem();
        m_server = m_mediator.addRandomServer(m_system, boost::none, false);

        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(m_system.id, m_server->serverId(), m_system.authKey));

        SocketGlobals::mediatorConnector().mockupAddress(m_mediator.stunEndpoint());
        SocketGlobals::mediatorConnector().enable(true);
    }

    std::unique_ptr<AbstractStreamServerSocket> cloudServerSocket()
    {
        auto serverSocket = std::make_unique<CloudServerSocket>(
            SocketGlobals::mediatorConnector().systemConnection());

        serverSocket->setSupportedConnectionMethods(hpm::api::ConnectionMethod::reverseConnect);
        NX_CRITICAL(serverSocket->registerOnMediatorSync() == hpm::api::ResultCode::ok);
        return std::unique_ptr<AbstractStreamServerSocket>(std::move(serverSocket));
    }

    void enableReveseConnectionsOnClient(boost::optional<size_t> poolSize = boost::none)
    {
        auto& pool = SocketGlobals::tcpReversePool();
        pool.setPoolSize(poolSize);
        pool.setKeepAliveOptions(KeepAliveOptions(60, 10, 3));
        ASSERT_TRUE(pool.start(HostAddress::localhost, 0, true));
    }

    hpm::MediatorFunctionalTest m_mediator;
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<nx::hpm::MediaServerEmulator> m_server;
};

TEST_F(TcpReverseConnectTest, SimpleSyncClientServer)
{
    // Client binds 1st, meditor resieves indication on listen:
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    network::test::socketSimpleSync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(m_server->fullName()));
}

TEST_F(TcpReverseConnectTest, SimpleSyncServerClient)
{
    // Meditor starts to listen 1st, client initiates indication by it's bind:
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();
    enableReveseConnectionsOnClient();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    network::test::socketSimpleSync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(m_server->fullName()));
}

TEST_F(TcpReverseConnectTest, SimpleSyncClientSystem)
{
    // Client binds 1st, meditor resieves indication on listen:
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    network::test::socketSimpleSync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(m_system.id));
}

TEST_F(TcpReverseConnectTest, SimpleAsyncClientSystem)
{
    // Client binds 1st, meditor resieves indication on listen:
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    network::test::socketSimpleAsync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(m_system.id));
}

TEST_F(TcpReverseConnectTest, Load)
{
    // Wait for some NXRC connections to be estabilished
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();
    enableReveseConnectionsOnClient();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const std::chrono::seconds testDuration(7 * utils::TestOptions::timeoutMultiplier());
    const int maxSimultaneousConnections = 25;
    const int bytesToSendThroughConnection = 1024 * 1024;

    network::test::RandomDataTcpServer server(
        network::test::TestTrafficLimitType::none,
        bytesToSendThroughConnection,
        network::test::TestTransmissionMode::spam);

    server.setServerSocket(std::move(serverSocket));
    ASSERT_TRUE(server.start());

    network::test::ConnectionsGenerator connectionsGenerator(
        SocketAddress(QString::fromUtf8(m_server->fullName()), 0),
        maxSimultaneousConnections,
        network::test::TestTrafficLimitType::incoming,
        bytesToSendThroughConnection,
        network::test::ConnectionsGenerator::kInfiniteConnectionCount,
        network::test::TestTransmissionMode::spam);
    connectionsGenerator.start();

    std::this_thread::sleep_for(testDuration);
    connectionsGenerator.pleaseStopSync();

    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0);
    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0);
    server.pleaseStopSync();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
