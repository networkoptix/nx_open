// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/ssl/context.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/stun_async_client_acceptance_tests.h>
#include <nx/network/test_support/stun_simple_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

static constexpr int kTestMethodNumber = stun::MethodType::userMethod + 1;

class StunClient:
    public ::testing::Test
{
public:
    StunClient()
    {
        initializeStunClient();
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
    nx::network::stun::MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    std::shared_ptr<nx::network::stun::AsyncClient> getStunClient()
    {
        return m_stunClient;
    }

    void givenRegularStunServer()
    {
        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            [this](auto&&... args) { sendResponse(std::forward<decltype(args)>(args)...); });

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
        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            [this](auto&&... args) { closeConnection(std::forward<decltype(args)>(args)...); });

        startServer();
    }

    void givenServerThatRandomlyClosesConnections()
    {
        dispatcher().registerRequestProcessor(
            kTestMethodNumber,
            [this](auto&&... args) { randomlyCloseConnection(std::forward<decltype(args)>(args)...); });

        startServer();
    }

    void givenConnectedClient()
    {
        m_stunClient->connect(
            nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName)
                .setEndpoint(m_serverEndpoint));
    }

    void givenTcpConnectionToTheServer()
    {
        m_tcpConnectionToTheServer = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_tcpConnectionToTheServer->connect(
            m_server->address(), nx::network::kNoTimeout));

        ASSERT_TRUE(m_tcpConnectionToTheServer->setNonBlockingMode(true));
    }

    void givenStunClientWithPredefinedConnection()
    {
        initializeStunClient(std::exchange(m_tcpConnectionToTheServer, nullptr));
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
        ASSERT_NE(SystemError::noError, m_connectionClosedPromise.pop());
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

    void assertConnectFailsIfUrlSchemeIs(const std::string& urlScheme)
    {
        assertConnectUsingUrlSchemeReportsSpecificResult(urlScheme, SystemError::invalidData);
    }

    void assertConnectSucceededIfUrlSchemeIs(const std::string& urlScheme)
    {
        assertConnectUsingUrlSchemeReportsSpecificResult(urlScheme, SystemError::noError);
    }

    void assertConnectUsingUrlSchemeReportsSpecificResult(
        const std::string& urlScheme,
        SystemError::ErrorCode sysErrorCode)
    {
        if (m_stunClient)
        {
            m_stunClient->pleaseStopSync();
            m_stunClient.reset();
        }
        initializeStunClient();

        auto url = serverUrl();
        url.setScheme(urlScheme);
        ASSERT_EQ(sysErrorCode, connectToUrl(url)) << urlScheme;
    }

private:
    static constexpr int kNumberOfRequestsToSend = 1024;

    bool m_connectionCloseHandlerInstalled = false;
    std::shared_ptr<nx::network::stun::AsyncClient> m_stunClient;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionClosedPromise;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_requestResult;
    SocketAddress m_serverEndpoint;
    nx::network::stun::MessageDispatcher m_dispatcher;
    std::unique_ptr<stun::SocketServer> m_server;
    std::unique_ptr<nx::network::TCPSocket> m_tcpConnectionToTheServer;

    void initializeStunClient(std::unique_ptr<AbstractStreamSocket> connection = nullptr)
    {
        stun::AbstractAsyncClient::Settings settings;
        settings.sendTimeout = kNoTimeout;
        settings.recvTimeout = kNoTimeout;

        m_stunClient = std::make_shared<nx::network::stun::AsyncClient>(
            std::move(connection),
            settings);
    }

    void closeConnection(nx::network::stun::MessageContext ctx)
    {
        ctx.connection->close();
    }

    void sendResponse(nx::network::stun::MessageContext ctx)
    {
        nx::network::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                ctx.message.header.method,
                ctx.message.header.transactionId));
        ctx.connection->sendMessage(std::move(response));
    }

    void storeRequestResult(
        SystemError::ErrorCode sysErrorCode,
        stun::Message /*response*/)
    {
        m_requestResult.push(sysErrorCode);
    }

    void randomlyCloseConnection(nx::network::stun::MessageContext ctx)
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
        {
            ctx.connection->close();
            return;
        }

        nx::network::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                ctx.message.header.method,
                ctx.message.header.transactionId));
        ctx.connection->sendMessage(std::move(response));
    }

    void startServer()
    {
        m_server = std::make_unique<stun::SocketServer>(
            &m_dispatcher,
            ssl::Context::instance());

        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText();
        ASSERT_TRUE(m_server->listen()) << SystemError::getLastOSErrorText();

        m_serverEndpoint = m_server->address();
    }

    void connectClientToTheServer()
    {
        if (!m_connectionCloseHandlerInstalled)
        {
            m_connectionCloseHandlerInstalled = true;
            m_stunClient->addOnConnectionClosedHandler(
                [this](SystemError::ErrorCode closeReason)
                {
                    m_connectionClosedPromise.push(closeReason);
                },
                this);
        }

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
        return nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(
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

    assertConnectFailsIfUrlSchemeIs(nx::network::http::kUrlSchemeName);
    assertConnectSucceededIfUrlSchemeIs(nx::network::stun::kUrlSchemeName);
    assertConnectSucceededIfUrlSchemeIs(nx::network::stun::kSecureUrlSchemeName);
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

    void givenUsersConnectedToClient(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            auto& user = m_users.emplace_back(
                std::make_unique<stun::AsyncClientUser>(getStunClient()));

            user->setOnConnectionClosedHandler(
                [this, user = user.get()](SystemError::ErrorCode /*closeReason*/)
                {
                    m_connectionClosedEvents.push(user);
                });
        }
    }

    void thenAllUsersReceiveConnectionClosedEvent()
    {
        std::set<AsyncClientUser*> closedUsers;
        for (int i = 0; i < static_cast<int>(m_users.size()); ++i)
        {
            auto closedUser = m_connectionClosedEvents.pop();
            closedUsers.emplace(closedUser);

            ASSERT_TRUE(
                std::any_of(
                    m_users.begin(), m_users.end(),
                    [closedUser](const auto& user) { return user.get() == closedUser; }));
        }

        ASSERT_EQ(m_users.size(), closedUsers.size());
    }

private:
    std::vector<std::unique_ptr<stun::AsyncClientUser>> m_users;
    nx::utils::SyncQueue<stun::AsyncClientUser*> m_connectionClosedEvents;
};

TEST_F(StunClientUser, correct_cancellation)
{
    givenRegularStunServer();
    givenClientConnectedToServer();
    assertIfResponseReportedAfterClientRemoval();
}

TEST_F(StunClientUser, multiple_close_handlers_multiple_times)
{
    givenUsersConnectedToClient(10);

    givenServerThatBreaksConnectionAfterReceivingRequest();

    for (int i = 0; i < 10; ++i)
    {
        givenClientConnectedToServer();

        whenClientSendsRequestToTheServer();

        thenAllUsersReceiveConnectionClosedEvent();
    }
}

//-------------------------------------------------------------------------------------------------

namespace {

struct AsyncClientTestTypes
{
    using ClientType = stun::AsyncClient;
    using ServerType = SimpleServer;
};

} // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(
    StunAsyncClient,
    StunAsyncClientAcceptanceTest,
    AsyncClientTestTypes);

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
