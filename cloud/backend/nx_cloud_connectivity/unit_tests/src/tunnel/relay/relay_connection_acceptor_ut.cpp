#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_upgrade.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::cloud::relay::test {

using namespace nx::cloud::relay;

class RelayTest:
    public ::testing::Test
{
public:
    RelayTest()
    {
        m_factoryFunctionBak =
            nx::cloud::relay::api::ClientFactory::instance().setCustomFunc(
                [this](auto&&... args) { return createClient(std::move(args)...); });

        setKeepAliveReported(true);
    }

    ~RelayTest()
    {
        if (m_factoryFunctionBak)
        {
            nx::cloud::relay::api::ClientFactory::instance().setCustomFunc(
                std::move(*m_factoryFunctionBak));
            m_factoryFunctionBak = std::nullopt;
        }
    }

protected:
    enum class ServerType
    {
        happy,
        unhappy,
    };

    ServerType m_serverType = ServerType::happy;
    utils::SyncQueue<std::unique_ptr<nx::network::AbstractStreamSocket>>
        m_listeningPeerConnectionsToRelay;

    virtual void SetUp() override
    {
        m_testHttpServer.registerRequestProcessorFunc(
            api::kServerIncomingConnectionsPath,
            [this](auto&&... args) { processIncomingConnection(std::move(args)...); });

        ASSERT_TRUE(m_testHttpServer.bindAndListen());

        m_relayServerUrl = nx::utils::Url(lm("http://%1/").arg(m_testHttpServer.serverAddress()));
        m_relayServerUrl.setUserName("server1.system1");
    }

    void setRelayToReportPreemptiveConnectionCount(int count)
    {
        m_beginListeningResponse.preemptiveConnectionCount = count;
    }

    nx::utils::Url relayServerUrl() const
    {
        return m_relayServerUrl;
    }

    api::BeginListeningResponse lastReportedBeginListeningResponse() const
    {
        return m_beginListeningResponse;
    }

    void setKeepAliveReported(bool keepAliveReportedFlag)
    {
        if (!keepAliveReportedFlag)
        {
            m_beginListeningResponse.keepAliveOptions = std::nullopt;
        }
        else
        {
            m_beginListeningResponse.keepAliveOptions = KeepAliveOptions();
            m_beginListeningResponse.keepAliveOptions->inactivityPeriodBeforeFirstProbe =
                std::chrono::seconds(3);
            m_beginListeningResponse.keepAliveOptions->probeSendPeriod =
                std::chrono::seconds(3);
            m_beginListeningResponse.keepAliveOptions->probeCount = 3;
        }
    }

private:
    nx::network::http::TestHttpServer m_testHttpServer;
    nx::utils::Url m_relayServerUrl;
    api::BeginListeningResponse m_beginListeningResponse;
    std::optional<nx::cloud::relay::api::ClientFactory::Function> m_factoryFunctionBak;

    void processIncomingConnection(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        if (m_serverType != ServerType::happy)
        {
            requestContext.connection->closeConnection(SystemError::connectionReset);
            return;
        }

        if (requestContext.request.requestLine.method == nx::network::http::Method::post)
        {
            api::serializeToHeaders(
                &requestContext.response->headers,
                m_beginListeningResponse);
        }

        nx::network::http::RequestResult requestResult(
            nx::network::http::StatusCode::switchingProtocols);
        requestResult.connectionEvents.onResponseHasBeenSent =
            [this](auto&&... args) { saveConnection(std::move(args)...); };
        completionHandler(std::move(requestResult));
    }

    void saveConnection(nx::network::http::HttpServerConnection* const connection)
    {
        auto socket = connection->takeSocket();
        NX_ASSERT(socket->isInSelfAioThread());
        socket->cancelIOSync(aio::etNone);
        m_listeningPeerConnectionsToRelay.push(std::move(socket));
    }

    std::unique_ptr<nx::cloud::relay::api::Client> createClient(
        const nx::utils::Url& relayUrl)
    {
        return std::make_unique<nx::cloud::relay::api::ClientOverHttpUpgrade>(
            relayUrl, nullptr);
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
        m_clientEndpoint.address = nx::network::HostAddress::localhost;
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
        m_connection->waitForOriginatorToStartUsingConnection(
            [this](auto&&... args) { onConnectionActivated(std::move(args)...); });
    }

    void whenRelayActivatesConnection()
    {
        m_serverConnection = m_listeningPeerConnectionsToRelay.pop();

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
        m_serverConnection = m_listeningPeerConnectionsToRelay.pop();
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
        m_connection->connectToOriginator(
            [this](auto&&... args) { onConnectionToOriginatorCompletion(std::move(args)...); });
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
        ASSERT_EQ(
            lastReportedBeginListeningResponse(),
            m_connection->beginListeningResponse());
    }

    void thenNotificationIsIgnored()
    {
        // Just making sure nothing happens (like process crash, for example).
        std::this_thread::sleep_for(std::chrono::milliseconds(17));
    }

    void assertKeepAliveIsExpected()
    {
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;
        m_connection->executeInAioThreadSync(
            [this, &connection]()
            {
                connection = std::move(m_connection->takeSocket());
                m_connection->pleaseStopSync();
            });

        std::optional<KeepAliveOptions> keepAliveOptions;
        ASSERT_TRUE(connection->getKeepAlive(&keepAliveOptions))
            << SystemError::getLastOSErrorText().toStdString();

        auto expectedKeepAliveOptions =
            lastReportedBeginListeningResponse().keepAliveOptions;
        if (expectedKeepAliveOptions)
            expectedKeepAliveOptions->resetUnsupportedFieldsToSystemDefault();

        ASSERT_EQ(
            static_cast<bool>(expectedKeepAliveOptions),
            static_cast<bool>(keepAliveOptions));
        if (keepAliveOptions)
        {
            ASSERT_EQ(*expectedKeepAliveOptions, *keepAliveOptions);
        }
    }

private:
    utils::SyncQueue<SystemError::ErrorCode> m_connectResult;
    utils::SyncQueue<SystemError::ErrorCode> m_activateConnectionResult;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_serverConnection;
    nx::network::SocketAddress m_clientEndpoint;
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
        m_acceptor->acceptAsync(
            [this](auto&&... args) { onAccepted(std::move(args)...); });
    }

    void whenRelayClosesConnectionsNumberOf(int count)
    {
        for (int i = 0; i < count; ++i)
            m_listeningPeerConnectionsToRelay.pop();
    }

    void thenAcceptorIsRegisteredOnRelay()
    {
        auto connection = m_listeningPeerConnectionsToRelay.pop();
        ASSERT_NE(nullptr, connection.get());
        connection->pleaseStopSync();
    }

    void thenAcceptorKeepsEstablishedConnectionCountOf(std::size_t count)
    {
        while (m_listeningPeerConnectionsToRelay.size() < count)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Ensuring peer does not establish more connections.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_EQ(count, m_listeningPeerConnectionsToRelay.size());
    }

private:
    std::unique_ptr<nx::network::cloud::relay::ConnectionAcceptor> m_acceptor;

    void onAccepted(
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<nx::network::AbstractStreamSocket> /*streamSocket*/)
    {
    }
};

TEST_F(RelayConnectionAcceptor, registers_on_relay)
{
    whenStartedAccepting();
    thenAcceptorIsRegisteredOnRelay();
}

TEST_F(RelayConnectionAcceptor, takes_into_account_preemptive_connection_count)
{
    constexpr int kPreemptiveConnectionCount = 2;

    setRelayToReportPreemptiveConnectionCount(kPreemptiveConnectionCount);

    whenStartedAccepting();
    whenRelayClosesConnectionsNumberOf(
        api::BeginListeningResponse().preemptiveConnectionCount);

    thenAcceptorKeepsEstablishedConnectionCountOf(kPreemptiveConnectionCount);
}

} // namespace nx::network::cloud::relay::test
