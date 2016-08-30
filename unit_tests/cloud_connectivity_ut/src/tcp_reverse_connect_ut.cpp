#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

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
        m_server = m_mediator.addRandomServer(m_system, false);

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
        ASSERT_TRUE(SocketGlobals::tcpReversePool()
            .start(SocketAddress(HostAddress::localhost, 0), true /* waits for registration */));

        SocketGlobals::tcpReversePool().setPoolSize(poolSize);
    }

    hpm::MediatorFunctionalTest m_mediator;
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<nx::hpm::MediaServerEmulator> m_server;
};

TEST_F(TcpReverseConnectTest, SimpleClientServer)
{
    // Client binds 1st, meditor resieves indication on listen:
    enableReveseConnectionsOnClient();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::seconds(1));
    network::test::socketSimpleSync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(HostAddress::localhost, 0),
        SocketAddress(m_server->fullName()));
}

// TODO: #mux Enable and fix
TEST_F(TcpReverseConnectTest, DISABLED_SimpleServerClient)
{
    // Meditor starts to listen 1st, client initiates indication by it's bind:
    std::unique_ptr<AbstractStreamServerSocket> serverSocket = cloudServerSocket();
    enableReveseConnectionsOnClient();

    // Wait some time to let reverse connections to be estabilished:
    std::this_thread::sleep_for(std::chrono::seconds(1));
    network::test::socketSimpleSync(
        [&](){ return std::move(serverSocket); },
        [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
        SocketAddress(HostAddress::localhost, 0),
        SocketAddress(m_server->fullName()));
}

// TODO: #mux Make a lot more tests

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
