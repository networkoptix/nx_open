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

class RelayTest:
    public ::testing::Test
{
public:
    RelayTest()
    {
        setKeepAliveReported(true);
    }

protected:
    enum class ServerType
    {
        happy,
        unhappy,
    };

    ServerType m_serverType = ServerType::happy;
    utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverConnections;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_testHttpServer.registerRequestProcessorFunc(
            api::kServerIncomingConnectionsPath,
            std::bind(&RelayTest::processIncomingConnection, this,
                _1, _2, _3, _4, _5));

        ASSERT_TRUE(m_testHttpServer.bindAndListen());

        m_relayServerUrl = QUrl(lm("http://%1/").arg(m_testHttpServer.serverAddress()));
        m_relayServerUrl.setUserName("server1.system1");
    }

    QUrl relayServerUrl() const
    {
        return m_relayServerUrl;
    }

    api::BeginListeningResponse prevReportedBeginListeningResponse() const
    {
        return m_beginListeningResponse;
    }

    void setKeepAliveReported(bool val)
    {
        if (!val)
        {
            m_beginListeningResponse.keepAliveOptions.reset();
        }
        else
        {
            m_beginListeningResponse.keepAliveOptions = KeepAliveOptions();
            m_beginListeningResponse.keepAliveOptions->inactivityPeriodBeforeFirstProbe =
                std::chrono::seconds(3);
        }
    }

private:
    TestHttpServer m_testHttpServer;
    QUrl m_relayServerUrl;
    api::BeginListeningResponse m_beginListeningResponse;

    void processIncomingConnection(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        if (m_serverType != ServerType::happy)
        {
            connection->closeConnection(SystemError::connectionReset);
            return;
        }

        if (request.requestLine.method == nx_http::Method::post)
            api::serializeToHeaders(&response->headers, m_beginListeningResponse);

        nx_http::RequestResult requestResult(
            nx_http::StatusCode::switchingProtocols);
        requestResult.connectionEvents.onResponseHasBeenSent =
            std::bind(&RelayTest::saveConnection, this, _1);
        completionHandler(std::move(requestResult));
    }

    void saveConnection(nx_http::HttpServerConnection* const connection)
    {
        auto socket = connection->takeSocket();
        NX_ASSERT(socket->isInSelfAioThread());
        socket->cancelIOSync(aio::etNone);
        m_serverConnections.push(std::move(socket));
    }
};

//-------------------------------------------------------------------------------------------------

class RelayReverseConnection:
    public RelayTest
{
    using base_type = RelayTest;

public:
    RelayReverseConnection()
    {
        m_clientEndpoint.address = HostAddress::localhost;
        m_clientEndpoint.port = nx::utils::random::number<int>(10000, 60000);
    }

    ~RelayReverseConnection()
    {
        if (m_connection)
            m_connection->pleaseStopSync();

        if (m_serverConnection)
            m_serverConnection->pleaseStopSync();
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
    }

    void givenActivatedConnection()
    {
        givenEstablishedConnection();

        whenWaitingForConnectionActivation();
        whenRelayActivatesConnection();

        thenConnectionIsActivated();
    }

    void whenWaitingForConnectionActivation()
    {
        using namespace std::placeholders;

        m_connection->waitForOriginatorToStartUsingConnection(
            std::bind(&RelayReverseConnection::onConnectionActivated, this, _1));
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
            std::bind(&RelayReverseConnection::onConnectionToOriginatorCompletion, this, _1));
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
        ASSERT_EQ(prevReportedBeginListeningResponse(), m_connection->beginListeningResponse());
    }

    void thenNotificationIsIgnored()
    {
        // Just making sure nothing happens (like process crash, for example).
        std::this_thread::sleep_for(std::chrono::milliseconds(17));
    }

    void assertKeepAliveIsExpected()
    {
        std::unique_ptr<AbstractStreamSocket> connection;
        m_connection->executeInAioThreadSync(
            [this, &connection]()
            {
                connection = std::move(m_connection->takeSocket());
                m_connection->pleaseStopSync();
            });

        boost::optional<KeepAliveOptions> keepAliveOptions;
        ASSERT_TRUE(connection->getKeepAlive(&keepAliveOptions))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_EQ(prevReportedBeginListeningResponse().keepAliveOptions, keepAliveOptions);
    }

private:
    utils::SyncQueue<SystemError::ErrorCode> m_connectResult;
    utils::SyncQueue<SystemError::ErrorCode> m_activateConnectionResult;
    std::unique_ptr<AbstractStreamSocket> m_serverConnection;
    SocketAddress m_clientEndpoint;
    nx::Buffer m_openTunnelNotificationBuffer;
    std::unique_ptr<detail::ReverseConnection> m_connection;

    virtual void SetUp() override
    {
        base_type::SetUp();

        m_connection = std::make_unique<detail::ReverseConnection>(relayServerUrl());
    }

    virtual void TearDown() override
    {
        m_connection->pleaseStopSync();
    }

    void onConnectionToOriginatorCompletion(SystemError::ErrorCode sysErrorCode)
    {
        m_connectResult.push(sysErrorCode);
    }

    void onConnectionActivated(SystemError::ErrorCode sysErrorCode)
    {
        m_activateConnectionResult.push(sysErrorCode);
    }
};

TEST_F(RelayReverseConnection, connects_to_the_relay_server)
{
    givenHappyRelayServer();
    whenConnectingToTheRelayServer();
    thenConnectionHasBeenEstablished();
}

TEST_F(RelayReverseConnection, reports_error_on_failure_to_connect)
{
    givenUnhappyRelayServer();
    whenConnectingToTheRelayServer();
    thenConnectionCouldNotBeenEstablished();
}

TEST_F(RelayReverseConnection, connection_is_activated)
{
    givenEstablishedConnection();

    whenWaitingForConnectionActivation();
    whenRelayActivatesConnection();

    thenConnectionIsActivated();
    thenConnectionProvidesStreamSocket();
}

TEST_F(RelayReverseConnection, reports_error_on_subsequent_connection_to_relay_failure)
{
    givenEstablishedConnection();

    whenWaitingForConnectionActivation();
    whenRelayClosesConnection();

    thenConnectionActivationErrorIsReported();
}

TEST_F(RelayReverseConnection, provides_client_peer_name_on_success)
{
    givenActivatedConnection();
    thenSocketForeignEndpointMatchesClientOne();
}

TEST_F(RelayReverseConnection, provides_relay_settings_on_success)
{
    givenActivatedConnection();
    thenRelaySettingsAreAvailable();
}

TEST_F(
    RelayReverseConnection,
    does_not_receive_notification_until_waitForOriginatorToStartUsingConnection_call)
{
    givenEstablishedConnection();
    whenRelayActivatesConnection();
    thenNotificationIsIgnored();
}

TEST_F(
    RelayReverseConnection,
    notification_is_sent_prior_to_waitForOriginatorToStartUsingConnection_call)
{
    givenEstablishedConnection();
    whenRelayActivatesConnection();
    thenNotificationIsIgnored();

    whenWaitingForConnectionActivation();
    thenConnectionIsActivated();
}

TEST_F(RelayReverseConnection, keep_alive_is_enabled)
{
    setKeepAliveReported(true);

    givenEstablishedConnection();
    assertKeepAliveIsExpected();
}

TEST_F(RelayReverseConnection, keep_alive_is_not_enabled)
{
    setKeepAliveReported(false);

    givenEstablishedConnection();
    assertKeepAliveIsExpected();
}

//-------------------------------------------------------------------------------------------------

class RelayConnectionAcceptor:
    public RelayTest
{
    using base_type = RelayTest;

public:
    ~RelayConnectionAcceptor()
    {
        if (m_acceptor)
            m_acceptor->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_acceptor =
            std::make_unique<nx::network::cloud::relay::ConnectionAcceptor>(
                relayServerUrl());
    }

    void whenStartedAccepting()
    {
        using namespace std::placeholders;

        m_acceptor->acceptAsync(
            std::bind(&RelayConnectionAcceptor::onAccepted, this, _1, _2));
    }

    void thenAcceptorIsRegisteredOnRelay()
    {
        m_serverConnections.pop();
    }

private:
    std::unique_ptr<nx::network::cloud::relay::ConnectionAcceptor> m_acceptor;

    void onAccepted(
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> /*streamSocket*/)
    {
    }
};

TEST_F(RelayConnectionAcceptor, registers_on_relay)
{
    whenStartedAccepting();
    thenAcceptorIsRegisteredOnRelay();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
