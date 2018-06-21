#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_upgrade.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>
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
    }

protected:
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

    void whenInvokeBeginListening()
    {
        using namespace std::placeholders;

        initializeHttpServerIfNeeded();
        m_client = std::make_unique<typename ClientTypeSet::Client>(
            m_baseUrl, nullptr);

        m_client->beginListening(
            ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            std::bind(&RelayApiClientAcceptance::saveBeginListeningCompletionResult, this, _1, _2, _3));
    }

    void thenClientUsedRightUrl()
    {
        ASSERT_EQ(api::ResultCode::ok, m_lastResultCode);
    }

    void thenRequestSucceded()
    {
        ASSERT_EQ(ResultCode::ok, m_requestResultQueue.pop());
    }

    void andBeginListeningResponseIsCorrect()
    {
        ASSERT_EQ(
            m_expectedBeginListeningResponse,
            m_prevBeginListeningResponse);
    }

    void enableAuthentication()
    {
        m_authenticator = QAuthenticator();
        m_authenticator->setUser("username");
        m_authenticator->setPassword("password");
    }

private:
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;
    std::unique_ptr<typename ClientTypeSet::Client> m_client;
    nx::utils::Url m_baseUrl;
    ResultCode m_lastResultCode = ResultCode::unknownError;
    boost::optional<QAuthenticator> m_authenticator;
    nx::utils::SyncQueue<ResultCode> m_requestResultQueue;
    BeginListeningResponse m_expectedBeginListeningResponse;
    BeginListeningResponse m_prevBeginListeningResponse;
    ClientTypeSet m_clientTypeSet;

    void saveBeginListeningCompletionResult(
        ResultCode resultCode,
        BeginListeningResponse response,
        std::unique_ptr<nx::network::AbstractStreamSocket> /*connection*/)
    {
        m_prevBeginListeningResponse = std::move(response);
        m_requestResultQueue.push(resultCode);
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
            baseUrlPath.toStdString(),
            "some_server_name");
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
    this->whenInvokeBeginListening();

    this->thenRequestSucceded();
    this->andBeginListeningResponseIsCorrect();
}

REGISTER_TYPED_TEST_CASE_P(RelayApiClientAcceptance,
    extends_path,
    base_url_has_trailing_slash,
    base_url_has_empty_path,
    base_url_is_a_root_path,
    uses_authentication,
    begin_listening_response_delivered_properly);

} // namespace nx::cloud::relay::api::test
