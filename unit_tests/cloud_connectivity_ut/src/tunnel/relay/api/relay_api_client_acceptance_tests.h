#pragma once

#include <array>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_upgrade.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::cloud::relay::api::test {

template<typename ClientTypeSet>
class RelayApiClientAcceptance:
    public ::testing::Test
{
public:
    ~RelayApiClientAcceptance()
    {
        if (m_client)
        {
            m_client->pleaseStopSync();
            m_client.reset();
        }

        m_httpServer.reset();
    }

protected:
    void enableAuthentication()
    {
        m_authenticator = QAuthenticator();
        m_authenticator->setUser("username");
        m_authenticator->setPassword("password");
    }

    void givenBaseUrlWithTrailingSlash()
    {
        initializeHttpServer("/test/");
    }

    void givenBaseUrlWithEmptyPath()
    {
        initializeHttpServer("/");
        m_baseUrl.setPath("");
    }

    void givenBaseUrlWithRootPath()
    {
        initializeHttpServer("/");
    }

    void givenBrokenBaseUrl()
    {
        m_baseUrl.setPath("/abra/kadabra");
    }

    void whenInvokedSomeRequest()
    {
        initializeHttpServerIfNeeded();
        m_client = std::make_unique<typename ClientTypeSet::Client>(
            m_baseUrl, nullptr);

        nx::utils::promise<void> done;
        m_client->startSession(
            "session_id",
            "some_server_name",
            [this, &done](
                ResultCode resultCode, CreateClientSessionResponse /*response*/)
            {
                m_lastResultCode = resultCode;
                done.set_value();
            });
        done.get_future().wait();
    }

    //---------------------------------------------------------------------------------------------
    // Server tunnel operations.

    void givenEstablishedServerTunnel()
    {
        whenInitiateServerTunnel();

        thenCreateServerTunnelSucceeded();
        andTunnelConnectionIsAvailable();
    }

    void whenInitiateServerTunnel()
    {
        using namespace std::placeholders;

        initializeHttpServerIfNeeded();
        m_client = std::make_unique<typename ClientTypeSet::Client>(
            m_baseUrl,
            std::bind(&RelayApiClientAcceptance::saveFeedback, this, _1));

        m_client->beginListening(
            ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            std::bind(&RelayApiClientAcceptance::saveBeginListeningCompletionResult, this,
                _1, _2, _3));
    }

    void whenServerTunnelFailed()
    {
        initializeHttpServerIfNeeded();

        givenBrokenBaseUrl();
        whenInitiateServerTunnel();
        thenCreateServerTunnelFailed();
    }

    void thenCreateServerTunnelSucceeded()
    {
        thenCreateServerTunnelCompleted();
        ASSERT_EQ(ResultCode::ok, m_prevCreateServerTunnelResult->resultCode);
    }

    void thenCreateServerTunnelFailed()
    {
        thenCreateServerTunnelCompleted();
        ASSERT_NE(ResultCode::ok, m_prevCreateServerTunnelResult->resultCode);
    }

    void thenCreateServerTunnelCompleted()
    {
        m_prevCreateServerTunnelResult = m_createServerTunnelResults.pop();
    }

    void andBeginListeningResponseIsCorrect()
    {
        ASSERT_EQ(
            m_expectedBeginListeningResponse,
            m_prevCreateServerTunnelResult->response);
    }

    //---------------------------------------------------------------------------------------------
    // Client tunnel operations.

    void whenInitiateClientTunnel()
    {
        using namespace std::placeholders;

        initializeHttpServerIfNeeded();

        m_client = std::make_unique<typename ClientTypeSet::Client>(
            m_baseUrl,
            std::bind(&RelayApiClientAcceptance::saveFeedback, this, _1));

        m_client->startSession(
            "sessionId",
            ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            std::bind(&RelayApiClientAcceptance::onCreateClientSessionCompleted, this, _1, _2));
    }

    void onCreateClientSessionCompleted(
        api::ResultCode resultCode,
        CreateClientSessionResponse response)
    {
        using namespace std::placeholders;

        if (resultCode != api::ResultCode::ok)
        {
            m_clientTunnelResults.push({resultCode, nullptr});
            return;
        }

        m_client->openConnectionToTheTargetHost(
            response.sessionId,
            std::bind(&RelayApiClientAcceptance::saveClientTunnelResult, this,
                _1, _2));
    }

    void saveClientTunnelResult(
        api::ResultCode resultCode,
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        m_clientTunnelResults.push({resultCode, std::move(connection)});
    }

    void whenClientTunnelFailed()
    {
        initializeHttpServerIfNeeded();

        givenBrokenBaseUrl();
        whenInitiateClientTunnel();
        thenCreateClientTunnelFailed();
    }

    void thenClientUsedRightUrl()
    {
        ASSERT_EQ(api::ResultCode::ok, m_lastResultCode);
    }

    void thenCreateClientTunnelSucceeded()
    {
        thenCreateClientTunnelCompleted();
        ASSERT_EQ(api::ResultCode::ok, m_prevClientTunnelResult->resultCode);
    }

    void andClientTunnelConnectionIsAvailable()
    {
        ASSERT_NE(nullptr, m_prevClientTunnelResult->connection);
    }

    void thenCreateClientTunnelFailed()
    {
        thenCreateClientTunnelCompleted();
        ASSERT_NE(api::ResultCode::ok, m_prevClientTunnelResult->resultCode);
    }

    void thenCreateClientTunnelCompleted()
    {
        m_prevClientTunnelResult = m_clientTunnelResults.pop();
    }

    //---------------------------------------------------------------------------------------------
    // Common.

    void andTunnelConnectionIsAvailable()
    {
        ASSERT_NE(nullptr, m_prevCreateServerTunnelResult->connection);
    }

    void thenFeedbackIsReported()
    {
        ASSERT_EQ(
            m_prevCreateServerTunnelResult
                ? m_prevCreateServerTunnelResult->resultCode
                : m_prevClientTunnelResult->resultCode,
            m_feedbackCodes.pop());
    }

    void assertDataExchangeGoesThroughConnection()
    {
        auto relaySideConnection = m_clientTypeSet.takeLastTunnelConnection();

        exchangeSomeData(
            m_prevCreateServerTunnelResult->connection.get(),
            relaySideConnection.get());
    }

private:
    struct CreateServerTunnelResult
    {
        ResultCode resultCode = ResultCode::ok;
        BeginListeningResponse response;
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;

        CreateServerTunnelResult() = default;
    };

    struct ClientTunnelResult
    {
        api::ResultCode resultCode;
        std::unique_ptr<network::AbstractStreamSocket> connection;

        ClientTunnelResult() = default;
    };

    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;
    std::unique_ptr<typename ClientTypeSet::Client> m_client;
    nx::utils::Url m_baseUrl;
    ResultCode m_lastResultCode = ResultCode::unknownError;
    boost::optional<QAuthenticator> m_authenticator;
    ClientTypeSet m_clientTypeSet;
    nx::utils::SyncQueue<ResultCode> m_feedbackCodes;

    nx::utils::SyncQueue<CreateServerTunnelResult> m_createServerTunnelResults;
    std::optional<CreateServerTunnelResult> m_prevCreateServerTunnelResult;
    BeginListeningResponse m_expectedBeginListeningResponse;

    nx::utils::SyncQueue<ClientTunnelResult> m_clientTunnelResults;
    std::optional<ClientTunnelResult> m_prevClientTunnelResult;

    void saveBeginListeningCompletionResult(
        ResultCode resultCode,
        BeginListeningResponse response,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection)
    {
        m_createServerTunnelResults.push(
            {resultCode, std::move(response), std::move(connection)});
    }

    void initializeHttpServerIfNeeded()
    {
        if (!m_httpServer)
            initializeHttpServer("/test");
    }

    void initializeHttpServer(QString baseUrlPath)
    {
        if (baseUrlPath.isEmpty())
            baseUrlPath = "/";

        prepareBeginListeningResponse();

        m_httpServer = std::make_unique<nx::network::http::TestHttpServer>();

        m_clientTypeSet.initializeHttpServer(
            m_httpServer.get(),
            baseUrlPath.toStdString());
        m_clientTypeSet.setBeginListeningResponse(m_expectedBeginListeningResponse);

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::utils::Url(lm("http://%1/%2")
            .args(m_httpServer->serverAddress(), baseUrlPath));

        if (m_authenticator)
        {
            m_httpServer->setAuthenticationEnabled(true);
            m_httpServer->registerUserCredentials(
                m_authenticator->user().toUtf8(),
                m_authenticator->password().toUtf8());
            m_baseUrl.setUserName(m_authenticator->user());
            m_baseUrl.setPassword(m_authenticator->password());
        }
    }

    void prepareBeginListeningResponse()
    {
        m_expectedBeginListeningResponse.preemptiveConnectionCount =
            nx::utils::random::number<int>(1, 99);
        if (nx::utils::random::number<int>(0, 1) > 0)
        {
            m_expectedBeginListeningResponse.keepAliveOptions = nx::network::KeepAliveOptions();
            m_expectedBeginListeningResponse.keepAliveOptions->probeCount =
                nx::utils::random::number<int>(1, 99);
            m_expectedBeginListeningResponse.keepAliveOptions->inactivityPeriodBeforeFirstProbe =
                std::chrono::seconds(nx::utils::random::number<int>(1, 99));
            m_expectedBeginListeningResponse.keepAliveOptions->probeSendPeriod =
                std::chrono::seconds(nx::utils::random::number<int>(1, 99));
        }
    }

    void saveFeedback(ResultCode resultCode)
    {
        m_feedbackCodes.push(resultCode);
    }

    void exchangeSomeData(
        network::AbstractStreamSocket* one,
        network::AbstractStreamSocket* two)
    {
        ASSERT_TRUE(one->setNonBlockingMode(false));
        ASSERT_TRUE(two->setNonBlockingMode(false));

        sendData(one, two);
        sendData(two, one);
    }

    void sendData(
        network::AbstractStreamSocket* from,
        network::AbstractStreamSocket* to)
    {
        constexpr int bufSize = 256;

        std::array<char, bufSize> sendBuf;
        std::generate(sendBuf.begin(), sendBuf.end(), &rand);
        ASSERT_EQ(
            (int) sendBuf.size(),
            from->send(sendBuf.data(), (unsigned int) sendBuf.size()));

        std::array<char, bufSize> recvBuf;
        ASSERT_EQ(
            (int) sendBuf.size(),
            to->recv(recvBuf.data(), (unsigned int) recvBuf.size(), 0));

        ASSERT_EQ(sendBuf, recvBuf);
    }
};

TYPED_TEST_CASE_P(RelayApiClientAcceptance);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(RelayApiClientAcceptance, extends_path)
{
    this->whenInvokedSomeRequest();
    this->thenClientUsedRightUrl();
}

TYPED_TEST_P(RelayApiClientAcceptance, base_url_has_trailing_slash)
{
    this->givenBaseUrlWithTrailingSlash();
    this->whenInvokedSomeRequest();
    this->thenClientUsedRightUrl();
}

TYPED_TEST_P(RelayApiClientAcceptance, base_url_has_empty_path)
{
    this->givenBaseUrlWithEmptyPath();
    this->whenInvokedSomeRequest();
    this->thenClientUsedRightUrl();
}

TYPED_TEST_P(RelayApiClientAcceptance, base_url_is_a_root_path)
{
    this->givenBaseUrlWithRootPath();
    this->whenInvokedSomeRequest();
    this->thenClientUsedRightUrl();
}

TYPED_TEST_P(RelayApiClientAcceptance, uses_authentication)
{
    this->enableAuthentication();

    this->whenInvokedSomeRequest();
    this->thenClientUsedRightUrl();
}

TYPED_TEST_P(RelayApiClientAcceptance, begin_listening_response_delivered_properly)
{
    this->whenInitiateServerTunnel();

    this->thenCreateServerTunnelSucceeded();
    this->andBeginListeningResponseIsCorrect();
}

TYPED_TEST_P(RelayApiClientAcceptance, sending_data_through_server_tunnel)
{
    this->givenEstablishedServerTunnel();
    this->assertDataExchangeGoesThroughConnection();
}

TYPED_TEST_P(RelayApiClientAcceptance, provides_feedback_about_server_tunnel)
{
    this->whenServerTunnelFailed();
    this->thenFeedbackIsReported();
}

TYPED_TEST_P(RelayApiClientAcceptance, establish_client_tunnel)
{
    this->whenInitiateClientTunnel();

    this->thenCreateClientTunnelSucceeded();
    this->andClientTunnelConnectionIsAvailable();
}

// TYPED_TEST_P(RelayApiClientAcceptance, sending_data_through_client_tunnel)

TYPED_TEST_P(RelayApiClientAcceptance, provides_feedback_about_client_tunnel)
{
    this->whenClientTunnelFailed();
    this->thenFeedbackIsReported();
}

REGISTER_TYPED_TEST_CASE_P(RelayApiClientAcceptance,
    extends_path,
    base_url_has_trailing_slash,
    base_url_has_empty_path,
    base_url_is_a_root_path,
    uses_authentication,
    begin_listening_response_delivered_properly,
    sending_data_through_server_tunnel,
    provides_feedback_about_server_tunnel,
    establish_client_tunnel,
    //sending_data_through_client_tunnel,
    provides_feedback_about_client_tunnel);

} // namespace nx::cloud::relay::api::test
