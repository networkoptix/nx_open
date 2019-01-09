#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/client.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/network/maintenance/log/server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::maintenance::log::test {

static constexpr char kBasePath[] = "/log";

class LogServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_logServer.registerRequestHandlers(
            kBasePath,
            &m_httpServer.httpMessageDispatcher());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);
    }

    void whenRequestLogConfiguration()
    {
        ASSERT_TRUE(m_httpClient.doGet(createRequestUrl(kLoggers)));
    }

    void thenRequestSucceeded()
    {
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void andActualConfigurationIsProvided()
    {
        const auto messageBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        auto loggers = QJson::deserialized<Loggers>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        // TODO: Validating loggers
    }

private:
    http::TestHttpServer m_httpServer;
    maintenance::log::Server m_logServer;
    http::HttpClient m_httpClient;

    nx::utils::Url createRequestUrl(const std::string& requestName)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kBasePath).appendPath(requestName).toUrl();
    }
};

TEST_F(LogServer, DISABLED_log_configuration_is_provided)
{
    whenRequestLogConfiguration();

    thenRequestSucceeded();
    andActualConfigurationIsProvided();
}

} // namespace nx::network::maintenance::log::test
