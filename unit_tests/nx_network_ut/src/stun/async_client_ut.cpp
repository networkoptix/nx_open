#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/sync_queue.h>

#include "stun_async_client_acceptance_tests.h"

namespace nx {
namespace stun {
namespace test {

static constexpr int kTestMethodNumber = stun::MethodType::userMethod + 1;

class StunClient:
    public ::testing::Test
{
public:
    StunClient()
    {
        m_stunClient = std::make_shared<nx::stun::AsyncClient>();
    }

    ~StunClient()
    {
        m_stunClient->pleaseStopSync();

        if (m_tcpConnectionToTheServer)
            m_tcpConnectionToTheServer->pleaseStopSync();

        if (m_server)
        {
            m_server->pleaseStopSync();
            m_server.reset();
        }
    }

protected:
    nx::stun::MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    std::shared_ptr<nx::stun::AsyncClient> getStunClient()
    {
        return m_stunClient;
    }

    void givenRegularStunServer()
    {
        using namespace std::placeholders;

        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&StunClient::sendResponse, this, _1, _2));

        startServer();
    }

    void givenClientConnectedToServer()
    {
        if (!m_server)
            startServer();

        connectClientToTheServer();
    }

    void givenServerThatBreaksConnectionAfterReceivingRequest()
    {
        using namespace std::placeholders;

        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&StunClient::closeConnection, this, _1, _2));

        startServer();
    }

    void givenServerThatRandomlyClosesConnections()
    {
        using namespace std::placeholders;

        dispatcher().registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&StunClient::randomlyCloseConnection, this, _1, _2));

        startServer();
    }

    void givenConnectedClient()
    {
        m_stunClient->connect(
            nx::network::url::Builder().setScheme(nx::stun::kUrlSchemeName).setEndpoint(m_serverEndpoint));
    }

    void givenTcpConnectionToTheServer()
    {
        m_tcpConnectionToTheServer = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_tcpConnectionToTheServer->connect(m_server->address()));
    }

    void givenStunClientWithPredefinedConnection()
    {
        m_stunClient = std::make_shared<nx::stun::AsyncClient>(
            std::move(m_tcpConnectionToTheServer));
    }

    void whenServerTerminatedAbruptly()
    {
        m_server->pleaseStopSync();
        m_server.reset();
    }

    void whenClientSendsRequestToTheServer()
    {
        sendRequest();
    }

    void whenIssuedMultipleRequests()
    {
        connectClientToTheServer();

        for (int i = 0; i < kNumberOfRequestsToSend; ++i)
            sendRequest();
    }

    void verifyClientProcessedConnectionCloseProperly()
    {
        ASSERT_NE(SystemError::noError, m_connectionClosedPromise.get_future().get());
    }

    void thenResponseIsReceived()
    {
        getNextRequestResult();
    }

    void thenErrorResultIsReported()
    {
        ASSERT_NE(SystemError::noError, getNextRequestResult());
    }

    void thenEveryRequestResultHasBeenReceived()
    {
        for (int i = 0; i < kNumberOfRequestsToSend; ++i)
            getNextRequestResult();
    }

    void assertConnectFailsIfUrlSchemeIs(const nx::String& urlScheme)
    {
        assertConnectUsingUrlSchemeReportsSpecificResult(urlScheme, SystemError::invalidData);
    }

    void assertConnectSucceededIfUrlSchemeIs(const nx::String& urlScheme)
    {
        assertConnectUsingUrlSchemeReportsSpecificResult(urlScheme, SystemError::noError);
    }

    void assertConnectUsingUrlSchemeReportsSpecificResult(
        const nx::String& urlScheme,
        SystemError::ErrorCode sysErrorCode)
    {
        if (m_stunClient)
        {
            m_stunClient->pleaseStopSync();
            m_stunClient.reset();
        }
        m_stunClient = std::make_shared<nx::stun::AsyncClient>();

        auto url = serverUrl();
        url.setScheme(urlScheme);
        ASSERT_EQ(sysErrorCode, connectToUrl(url)) << urlScheme.toStdString();
    }

private:
    static constexpr int kNumberOfRequestsToSend = 1024;

    std::shared_ptr<nx::stun::AsyncClient> m_stunClient;
    nx::utils::promise<SystemError::ErrorCode> m_connectionClosedPromise;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_requestResult;
    SocketAddress m_serverEndpoint;
    nx::stun::MessageDispatcher m_dispatcher;
    std::unique_ptr<stun::SocketServer> m_server;
    std::unique_ptr<nx::network::TCPSocket> m_tcpConnectionToTheServer;

    void closeConnection(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message /*message*/)
    {
        connection->close();
    }

    void sendResponse(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message request)
    {
        nx::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                request.header.method,
                request.header.transactionId));
        connection->sendMessage(std::move(response));
    }

    void storeRequestResult(
        SystemError::ErrorCode sysErrorCode,
        stun::Message /*response*/)
    {
        m_requestResult.push(sysErrorCode);
    }

    void randomlyCloseConnection(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message request)
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
        {
            connection->close();
            return;
        }

        nx::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                request.header.method,
                request.header.transactionId));
        connection->sendMessage(std::move(response));
    }

    void startServer()
    {
        m_server = std::make_unique<stun::SocketServer>(&m_dispatcher, false);

        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->listen()) << SystemError::getLastOSErrorText().toStdString();

        m_serverEndpoint = m_server->address();
    }

    void connectClientToTheServer()
    {
        m_stunClient->setOnConnectionClosedHandler(
            [this](SystemError::ErrorCode closeReason)
            {
                m_connectionClosedPromise.set_value(closeReason);
            });

        ASSERT_EQ(SystemError::noError, connectToUrl(serverUrl()));
        ASSERT_EQ(m_serverEndpoint, m_stunClient->remoteAddress());
    }

    SystemError::ErrorCode connectToUrl(const nx::utils::Url& url)
    {
        nx::utils::promise<SystemError::ErrorCode> connectedPromise;
        m_stunClient->connect(
            url,
            [&connectedPromise](SystemError::ErrorCode sysErrorCode)
            {
                connectedPromise.set_value(sysErrorCode);
            });
        return connectedPromise.get_future().get();
    }

    nx::utils::Url serverUrl() const
    {
        return nx::network::url::Builder().setScheme(nx::stun::kUrlSchemeName).setEndpoint(
            SocketAddress(HostAddress("localhost"), m_serverEndpoint.port));
    }

    void sendRequest()
    {
        sendRequest(m_stunClient.get());
    }

    template<typename AsyncClientType>
    void sendRequest(AsyncClientType* client)
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            kTestMethodNumber));
        client->sendRequest(
            std::move(request),
            std::bind(&StunClient::storeRequestResult, this, _1, _2));
    }

    SystemError::ErrorCode getNextRequestResult()
    {
        return m_requestResult.pop();
    }
};

TEST_F(StunClient, proper_cancellation_when_connection_terminated_by_remote_side)
{
    givenClientConnectedToServer();
    whenServerTerminatedAbruptly();
    verifyClientProcessedConnectionCloseProperly();
}

TEST_F(StunClient, request_result_correctly_reported_on_connection_break)
{
    givenServerThatBreaksConnectionAfterReceivingRequest();
    givenConnectedClient();
    whenClientSendsRequestToTheServer();
    thenErrorResultIsReported();
}

TEST_F(StunClient, every_request_result_is_delivered)
{
    givenServerThatRandomlyClosesConnections();
    whenIssuedMultipleRequests();
    thenEveryRequestResultHasBeenReceived();
}

TEST_F(StunClient, uses_prepared_connection)
{
    givenRegularStunServer();
    givenTcpConnectionToTheServer();
    givenStunClientWithPredefinedConnection();

    whenClientSendsRequestToTheServer();
    thenResponseIsReceived();
}

TEST_F(StunClient, only_proper_url_scheme_is_supported)
{
    givenRegularStunServer();

    assertConnectFailsIfUrlSchemeIs(nx_http::kUrlSchemeName);
    assertConnectSucceededIfUrlSchemeIs(nx::stun::kUrlSchemeName);
    assertConnectSucceededIfUrlSchemeIs(nx::stun::kSecureUrlSchemeName);
}

//-------------------------------------------------------------------------------------------------
// StunClientUser

class StunClientUser:
    public StunClient
{
protected:
    void assertIfResponseReportedAfterClientRemoval()
    {
        constexpr int kTestRunCount = 100;

        for (int i = 0; i < kTestRunCount; ++i)
        {
            std::atomic<bool> clientUserStopped(false);
            auto clientUser = std::make_unique<AsyncClientUser>(getStunClient());

            stun::Message request(stun::Header(
                stun::MessageClass::request,
                kTestMethodNumber));
            clientUser->sendRequest(
                std::move(request),
                [&clientUserStopped](SystemError::ErrorCode /*sysErrorCode*/, stun::Message /*response*/)
                {
                    ASSERT_FALSE(clientUserStopped.load());
                });

            std::this_thread::sleep_for(
                std::chrono::milliseconds(nx::utils::random::number<int>(0, 10)));

            nx::utils::promise<void> clientUserRemoved;
            clientUser->post(
                [&clientUser, &clientUserRemoved, &clientUserStopped]()
                {
                    clientUser->pleaseStopSync();
                    clientUserStopped = true;
                    clientUserRemoved.set_value();
                });
            clientUserRemoved.get_future().wait();
        }
    }
};

TEST_F(StunClientUser, correct_cancellation)
{
    givenRegularStunServer();
    givenClientConnectedToServer();
    assertIfResponseReportedAfterClientRemoval();
}

//-------------------------------------------------------------------------------------------------

namespace {

class TestServer:
    public stun::SocketServer
{
    using base_type = stun::SocketServer;

public:
    TestServer():
        base_type(&m_dispatcher, false)
    {
    }

    ~TestServer()
    {
        pleaseStopSync();
    }

    nx::utils::Url getServerUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::stun::kUrlSchemeName).setEndpoint(address());
    }

    nx::stun::MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    void sendIndicationThroughEveryConnection(nx::stun::Message message)
    {
        forEachConnection(
            nx::network::server::MessageSender<nx::stun::ServerConnection>(std::move(message)));
    }

private:
    nx::stun::MessageDispatcher m_dispatcher;
};

struct AsyncClientTestTypes
{
    using ClientType = stun::AsyncClient;
    using ServerType = TestServer;
};

} // namespace

INSTANTIATE_TYPED_TEST_CASE_P(
    StunAsyncClient,
    StunAsyncClientAcceptanceTest,
    AsyncClientTestTypes);

} // namespace test
} // namespace stun
} // namespace nx
