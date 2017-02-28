#include <gtest/gtest.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <nx/network/cloud/address_resolver.h>
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

class CloudConnect:
    public ::testing::Test
{
public:
    CloudConnect(int methods = hpm::api::ConnectionMethod::all):
        m_methods(methods)
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

        serverSocket->setSupportedConnectionMethods(m_methods);
        NX_CRITICAL(serverSocket->registerOnMediatorSync() == hpm::api::ResultCode::ok);
        return std::unique_ptr<AbstractStreamServerSocket>(std::move(serverSocket));
    }

protected:
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_server;

private:
    hpm::MediatorFunctionalTest m_mediator;
    const int m_methods;

    void init()
    {
        ASSERT_TRUE(mediator().startAndWaitUntilStarted());

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

class UdpHolePunching:
    public CloudConnect
{
public:
    UdpHolePunching():
        CloudConnect(hpm::api::ConnectionMethod::udpHolePunching)
    {
    }
};

NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE_EX(
    TEST_F, UdpHolePunching,
    [&](){ return cloudServerSocket(); },
    [](){ return std::make_unique<CloudStreamSocket>(AF_INET); },
    SocketAddress(m_server->fullName()));

TEST_F(UdpHolePunching, TransferSyncSsl)
{
    network::test::socketTransferSync(
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

    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0U);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0U);
    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0U);

    server.pleaseStopSync();
}

//-------------------------------------------------------------------------------------------------
// CloudConnectTunnel

class CloudConnectTunnel:
    public CloudConnect
{
public:
    CloudConnectTunnel():
        m_onTunnelClosedSubscriptionId(nx::utils::kInvalidSubscriptionId),
        m_tunnelOpened(false)
    {
        using namespace std::placeholders;

        SocketGlobals::outgoingTunnelPool().onTunnelClosedSubscription().subscribe(
            std::bind(&CloudConnectTunnel::onTunnelClosed, this, _1),
            &m_onTunnelClosedSubscriptionId);
    }

    ~CloudConnectTunnel()
    {
        SocketGlobals::outgoingTunnelPool().onTunnelClosedSubscription().removeSubscription(
            m_onTunnelClosedSubscriptionId);
        m_onTunnelClosedSubscriptionId = nx::utils::kInvalidSubscriptionId;

        if (m_serverSocket)
            m_serverSocket->pleaseStopSync();
    }

protected:
    SocketAddress serverAddress() const
    {
        return SocketAddress(QString::fromUtf8(m_server->fullName()), 0);
    }

    void initializeCloudServerSocket()
    {
        m_serverSocket = cloudServerSocket();
        m_serverSocket->acceptAsync(
            [](SystemError::ErrorCode /*errorCode*/, AbstractStreamSocket* newConnection)
            {
                delete newConnection;
            });
    }

    void tryOpenTunnel()
    {
        auto clientSocket = std::make_unique<CloudStreamSocket>(AF_INET);
        m_tunnelOpened = clientSocket->connect(serverAddress(), 2000);
        clientSocket.reset();
    }

    void openConnection()
    {
        auto clientSocket = std::make_unique<CloudStreamSocket>(AF_INET);
        ASSERT_TRUE(clientSocket->connect(serverAddress(), 3000));
    }

    void destroyServerSocketWithinAioThread()
    {
        nx::utils::promise<void> removed;
        m_serverSocket->post(
            [this, &removed]()
            {
                m_serverSocket.reset();
                removed.set_value();
            });
        removed.get_future().wait();
    }

    void waitForTunnelClosed()
    {
        if (!m_tunnelOpened)
            return;

        QnMutexLocker lock(&m_mutex);
        for (;;)
        {
            auto it = m_closedTunnels.find(serverAddress().address.toString());
            if (it != m_closedTunnels.end())
            {
                m_closedTunnels.erase(it);
                return;
            }
            m_cond.wait(lock.mutex());
        }
    }

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    nx::utils::SubscriptionId m_onTunnelClosedSubscriptionId;
    std::multiset<QString> m_closedTunnels;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    bool m_tunnelOpened;

    void onTunnelClosed(const QString& hostName)
    {
        QnMutexLocker lock(&m_mutex);
        m_closedTunnels.insert(hostName);
        m_cond.wakeAll();
    }
};

TEST_F(CloudConnectTunnel, cancellation)
{
    for (int i = 0; i < 10; ++i)
    {
        initializeCloudServerSocket();
        tryOpenTunnel();

        auto clientSocket = std::make_unique<CloudStreamSocket>(AF_INET);
        clientSocket->connectAsync(serverAddress(), [](SystemError::ErrorCode) {});

        destroyServerSocketWithinAioThread();

        clientSocket->pleaseStopSync();

        waitForTunnelClosed();
    }
}

TEST_F(CloudConnectTunnel, DISABLED_reconnect)
{
    for (int i = 0; i < 10; ++i)
    {
        initializeCloudServerSocket();
        openConnection();
        destroyServerSocketWithinAioThread();
        waitForTunnelClosed();
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
