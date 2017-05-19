#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>

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
        m_baseUrl = QUrl(lm("http://%1/test/").arg(m_httpServer->serverAddress()));
    }

    void givenBaseUrlWithEmptyPath()
    {
        initializeHttpServer("/");
        m_baseUrl = QUrl(lm("http://%1").arg(m_httpServer->serverAddress()));
    }

    void givenBaseUrlWithRootPath()
    {
        initializeHttpServer("/");
        m_baseUrl = QUrl(lm("http://%1/").arg(m_httpServer->serverAddress()));
    }

    void whenInvokedSomeRequest()
    {
        if (!m_httpServer)
            initializeHttpServer("/test");

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

    void thenClientUsedRightUrl()
    {
        ASSERT_EQ(api::ResultCode::ok, m_lastResultCode);
    }

private:
    std::unique_ptr<TestHttpServer> m_httpServer;
    std::unique_ptr<ClientImpl> m_client;
    QUrl m_baseUrl;
    ResultCode m_lastResultCode = ResultCode::unknownError;

    void initializeHttpServer(QString baseUrlPath)
    {
        if (baseUrlPath.isEmpty())
            baseUrlPath = "/";

        m_httpServer = std::make_unique<TestHttpServer>();

        const auto realPath = nx_http::rest::substituteParameters(
            kServerClientSessionsPath,
            {"some_server_name"});

        const auto handlerPath = 
            network::url::normalizePath(lm("%1%2").arg(baseUrlPath).arg(realPath).toQString());

        m_httpServer->registerStaticProcessor(
            handlerPath,
            "{\"result\": \"ok\"}",
            "application/json");

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = QUrl(lm("http://%1/%2").arg(m_httpServer->serverAddress()).arg(baseUrlPath));
    }
};

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

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
