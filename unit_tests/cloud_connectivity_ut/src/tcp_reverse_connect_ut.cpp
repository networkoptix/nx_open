#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connection_pool.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

const std::chrono::seconds kTunnelInactivityTimeout(1);

class TcpReverseConnectTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();
        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());

        m_system = m_mediator.addRandomSystem();
        m_server = m_mediator.addRandomServer(
            m_system, boost::none, hpm::ServerTweak::noListenToConnect);

        ASSERT_NE(nullptr, m_server);
        SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            url::Builder().setScheme("stun").setEndpoint(m_mediator.stunEndpoint()));
        SocketGlobals::cloud().mediatorConnector().enable(true);
    }

    std::unique_ptr<AbstractStreamServerSocket> cloudServerSocket(
        const std::unique_ptr<nx::hpm::MediaServerEmulator>& server)
    {
        auto serverSocket = std::make_unique<CloudServerSocket>(
            &server->mediatorConnector());
        serverSocket->setSupportedConnectionMethods(hpm::api::ConnectionMethod::reverseConnect);
        NX_CRITICAL(serverSocket->registerOnMediatorSync() == hpm::api::ResultCode::ok);
        return std::unique_ptr<AbstractStreamServerSocket>(std::move(serverSocket));
    }

    void enableReveseConnectionsOnClient(boost::optional<size_t> poolSize = boost::none)
    {
        auto& pool = SocketGlobals::cloud().tcpReversePool();
        pool.setPoolSize(poolSize);
        pool.setKeepAliveOptions(KeepAliveOptions(
            std::chrono::seconds(60), std::chrono::seconds(10), 3));
        ASSERT_TRUE(pool.start(HostAddress::localhost, 0, true));
    }

    void simpleTest(std::unique_ptr<AbstractStreamServerSocket> serverSocket, String hostName)
    {
        // Wait some time to let reverse connections to be estabilished.
        std::this_thread::sleep_for(kTunnelInactivityTimeout);

        network::test::socketTransferSync(
            [&]() { return std::move(serverSocket); },
            []() { return std::make_unique<CloudStreamSocket>(AF_INET); },
            SocketAddress(hostName));
    }

    hpm::MediatorFunctionalTest m_mediator;
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<nx::hpm::MediaServerEmulator> m_server;
};

TEST_F(TcpReverseConnectTest, SimpleSyncClientServer)
{
    // Client binds 1st, meditor resieves indication on listen.
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);
    simpleTest(std::move(serverSocket), m_server->fullName());
}

TEST_F(TcpReverseConnectTest, SimpleSyncServerClient)
{
    // Meditor starts to listen 1st, client initiates indication by it's bind.
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);
    enableReveseConnectionsOnClient();
    simpleTest(std::move(serverSocket), m_server->fullName());
}

TEST_F(TcpReverseConnectTest, SimpleSyncClientSystem)
{
    // Client binds 1st, meditor resieves indication on listen.
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);
    simpleTest(std::move(serverSocket), m_system.id);
}

TEST_F(TcpReverseConnectTest, SimpleMultiserver)
{
    // Test with 1 server.
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);
    enableReveseConnectionsOnClient();
    simpleTest(std::move(serverSocket), m_system.id);

    // Add 2nd server.
    const auto server2 = m_mediator.addRandomServer(
        m_system, boost::none, hpm::ServerTweak::noListenToConnect);
    std::unique_ptr<AbstractStreamServerSocket> serverSocket2 = cloudServerSocket(server2);

    // Suppose 1st server went offline.
    m_server.reset();

    // Cause old tunnel to expire to close by unsuccessful connect attempt.
    CloudStreamSocket socket(AF_INET);
    ASSERT_FALSE(socket.connect(m_system.id, 100));
    ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());

    // Ensure new tunnel to open and function normaly.
    simpleTest(std::move(serverSocket2), m_system.id);
}

TEST_F(TcpReverseConnectTest, TransferSyncSsl)
{
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);

    // Wait some time to let reverse connections to be estabilished.
    std::this_thread::sleep_for(kTunnelInactivityTimeout);

    network::test::socketTransferSync(
        [&]() { return std::make_unique<deprecated::SslServerSocket>(std::move(serverSocket), false); },
        []() { return std::make_unique<deprecated::SslSocket>(std::make_unique<CloudStreamSocket>(AF_INET), false); },
        SocketAddress(m_server->fullName()));
}

TEST_F(TcpReverseConnectTest, Load)
{
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket(m_server);
    enableReveseConnectionsOnClient();
    std::this_thread::sleep_for(kTunnelInactivityTimeout);

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

    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0U);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0U);
    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0U);
    server.pleaseStopSync();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
