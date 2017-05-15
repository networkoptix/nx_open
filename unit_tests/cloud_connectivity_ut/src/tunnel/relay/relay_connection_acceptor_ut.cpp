#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

using namespace nx::cloud::relay;

class CloudRelayReverseConnection:
    public ::testing::Test
{
public:
    CloudRelayReverseConnection()
    {
        m_clientEndpoint.address = HostAddress::localhost;
        m_clientEndpoint.port = nx::utils::random::number<int>(10000, 60000);
    }

protected:
    void givenHappyRelayServer()
    {
        m_serverType = ServerType::happy;
    }

    void givenUnhappyRelayServer()
    {
        m_serverType = ServerType::unhappy;
    }

    void givenEstablishedConnection()
    {
        givenHappyRelayServer();
        whenConnectingToTheRelayServer();
        thenConnectionHasBeenEstablished();

        whenWaitingForConnectionActivation();
    }

    void givenActivatedConnection()
    {
        givenEstablishedConnection();
        whenRelayActivatesConnection();
        thenConnectionIsActivated();
    }

    void whenWaitingForConnectionActivation()
    {
        using namespace std::placeholders;

        m_connection->waitForOriginatorToStartUsingConnection(
            std::bind(&CloudRelayReverseConnection::onConnectionActivated, this, _1));
    }

    void whenRelayActivatesConnection()
    {
        m_serverConnection = m_serverConnections.pop();

        api::OpenTunnelNotification openTunnelNotification;
        openTunnelNotification.setClientPeerName("test_client_name");
        openTunnelNotification.setClientEndpoint(m_clientEndpoint);
        m_openTunnelNotificationBuffer = openTunnelNotification.toHttpMessage().toString();
        m_serverConnection->sendAsync(
            m_openTunnelNotificationBuffer,
            [](SystemError::ErrorCode, std::size_t) {});
    }

    void whenRelayClosesConnection()
    {
        m_serverConnection = m_serverConnections.pop();
        m_serverConnection->pleaseStopSync();
        m_serverConnection.reset();
    }

    void thenConnectionIsActivated()
    {
        ASSERT_EQ(SystemError::noError, m_activateConnectionResult.pop());
    }

    void thenConnectionProvidesStreamSocket()
    {
        ASSERT_NE(nullptr, m_connection->takeSocket());
    }

    void whenConnectingToTheRelayServer()
    {
        using namespace std::placeholders;

        m_connection->connectToOriginator(
            std::bind(&CloudRelayReverseConnection::onConnectionToOriginatorCompletion, this, _1));
    }

    void thenConnectionHasBeenEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_connectResult.pop());
    }

    void thenConnectionCouldNotBeenEstablished()
    {
        ASSERT_NE(SystemError::noError, m_connectResult.pop());
    }

    void thenConnectionActivationErrorIsReported()
    {
        ASSERT_NE(SystemError::noError, m_activateConnectionResult.pop());
    }

    void thenSocketForeignEndpointMatchesClientOne()
    {
        auto streamSocket = m_connection->takeSocket();
        // TODO: #ak Remove toString from the following ASSERT_EQ after CLOUD-1124.
        ASSERT_EQ(
            m_clientEndpoint.toString(),
            streamSocket->getForeignAddress().toString());
    }

    void thenRelaySettingsAreAvailable()
    {
        ASSERT_EQ(m_expectedSettings, m_connection->beginListeningResponse());
    }

private:
    enum class ServerType
    {
        happy,
        unhappy,
    };

    TestHttpServer m_testHttpServer;
    ServerType m_serverType = ServerType::happy;
    std::unique_ptr<detail::ReverseConnection> m_connection;
    utils::SyncQueue<SystemError::ErrorCode> m_connectResult;
    utils::SyncQueue<SystemError::ErrorCode> m_activateConnectionResult;
    utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverConnections;
    std::unique_ptr<AbstractStreamSocket> m_serverConnection;
    SocketAddress m_clientEndpoint;
    nx::Buffer m_openTunnelNotificationBuffer;
    nx::cloud::relay::api::BeginListeningResponse m_expectedSettings;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_testHttpServer.registerRequestProcessorFunc(
            api::kServerIncomingConnectionsPath,
            std::bind(&CloudRelayReverseConnection::processIncomingConnection, this,
                _1, _2, _3, _4, _5));

        ASSERT_TRUE(m_testHttpServer.bindAndListen());

        QUrl relayServerUrl(lm("http://%1/").arg(m_testHttpServer.serverAddress()));
        relayServerUrl.setUserName("server1.system1");
        m_connection = std::make_unique<detail::ReverseConnection>(relayServerUrl);
    }

    virtual void TearDown() override
    {
        m_connection->pleaseStopSync();
    }

    void processIncomingConnection(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        if (m_serverType != ServerType::happy)
        {
            connection->closeConnection(SystemError::connectionReset);
            return;
        }

        nx_http::RequestResult requestResult(
            nx_http::StatusCode::switchingProtocols);
        requestResult.connectionEvents.onResponseHasBeenSent =
            std::bind(&CloudRelayReverseConnection::saveConnection, this, _1);
        completionHandler(std::move(requestResult));
    }

    void onConnectionToOriginatorCompletion(SystemError::ErrorCode sysErrorCode)
    {
        m_connectResult.push(sysErrorCode);
    }

    void onConnectionActivated(SystemError::ErrorCode sysErrorCode)
    {
        m_activateConnectionResult.push(sysErrorCode);
    }

    void saveConnection(nx_http::HttpServerConnection* const connection)
    {
        m_serverConnections.push(connection->takeSocket());
    }
};

TEST_F(CloudRelayReverseConnection, connects_to_the_relay_server)
{
    givenHappyRelayServer();
    whenConnectingToTheRelayServer();
    thenConnectionHasBeenEstablished();
}

TEST_F(CloudRelayReverseConnection, reports_error_on_failure_to_connect)
{
    givenUnhappyRelayServer();
    whenConnectingToTheRelayServer();
    thenConnectionCouldNotBeenEstablished();
}

TEST_F(CloudRelayReverseConnection, connection_is_activated)
{
    givenEstablishedConnection();

    whenRelayActivatesConnection();

    thenConnectionIsActivated();
    thenConnectionProvidesStreamSocket();
}

TEST_F(CloudRelayReverseConnection, reports_error_on_subsequent_connection_to_relay_failure)
{
    givenEstablishedConnection();
    whenRelayClosesConnection();
    thenConnectionActivationErrorIsReported();
}

TEST_F(CloudRelayReverseConnection, provides_client_peer_name_on_success)
{
    givenActivatedConnection();
    thenSocketForeignEndpointMatchesClientOne();
}

TEST_F(CloudRelayReverseConnection, provides_relay_settings_on_success)
{
    givenActivatedConnection();
    thenRelaySettingsAreAvailable();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
