#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace test {

class RelayApiClient:
    public ::testing::Test
{
public:
    ~RelayApiClient()
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
        m_client = std::make_unique<ClientImpl>(m_baseUrl);

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
        m_client = std::make_unique<ClientImpl>(m_baseUrl);

        m_client->beginListening(
            "peerName",
            std::bind(&RelayApiClient::saveBeginListeningCompletionResult, this, _1, _2, _3));
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
        ASSERT_EQ(m_expectedBeginListeningResponse, m_prevBeginListeningResponse);
    }

    void enableAuthentication()
    {
        m_authenticator = QAuthenticator();
        m_authenticator->setUser("username");
        m_authenticator->setPassword("password");
    }

private:
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;
    std::unique_ptr<ClientImpl> m_client;
    nx::utils::Url m_baseUrl;
    ResultCode m_lastResultCode = ResultCode::unknownError;
    boost::optional<QAuthenticator> m_authenticator;
    nx::utils::SyncQueue<ResultCode> m_requestResultQueue;
    BeginListeningResponse m_expectedBeginListeningResponse;
    BeginListeningResponse m_prevBeginListeningResponse;

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
        using namespace std::placeholders;

        if (baseUrlPath.isEmpty())
            baseUrlPath = "/";

        m_httpServer = std::make_unique<nx::network::http::TestHttpServer>();

        const auto realPath = nx::network::http::rest::substituteParameters(
            kServerClientSessionsPath,
            {"some_server_name"});

        const auto handlerPath =
            network::url::normalizePath(lm("%1%2").args(baseUrlPath, realPath).toQString());

        m_httpServer->registerStaticProcessor(
            handlerPath,
            "{\"result\": \"ok\"}",
            "application/json");

        m_httpServer->registerRequestProcessorFunc(
            network::url::normalizePath(
                lm("%1/%2").args(baseUrlPath, kServerIncomingConnectionsPath).toQString()),
            std::bind(&RelayApiClient::beginListeningHandler, this, _1, _2, _3, _4, _5));

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::utils::Url(lm("http://%1/%2").arg(m_httpServer->serverAddress()).arg(baseUrlPath));

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

    void beginListeningHandler(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
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

        serializeToHeaders(&response->headers, m_expectedBeginListeningResponse);

        completionHandler(nx::network::http::StatusCode::switchingProtocols);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(RelayApiClient, extends_path)
{
    whenInvokedSomeRequest();
    thenClientUsedRightUrl();
}

TEST_F(RelayApiClient, base_url_has_trailing_slash)
{
    givenBaseUrlWithTrailingSlash();
    whenInvokedSomeRequest();
    thenClientUsedRightUrl();
}

TEST_F(RelayApiClient, base_url_has_empty_path)
{
    givenBaseUrlWithEmptyPath();
    whenInvokedSomeRequest();
    thenClientUsedRightUrl();
}

TEST_F(RelayApiClient, base_url_is_a_root_path)
{
    givenBaseUrlWithRootPath();
    whenInvokedSomeRequest();
    thenClientUsedRightUrl();
}

TEST_F(RelayApiClient, uses_authentication)
{
    enableAuthentication();

    whenInvokedSomeRequest();
    thenClientUsedRightUrl();
}

TEST_F(RelayApiClient, begin_listening_response_delivered_properly)
{
    whenInvokeBeginListening();

    thenRequestSucceded();
    andBeginListeningResponseIsCorrect();
}

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
