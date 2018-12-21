#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::maintenance::test {

class MaintenanceServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_maintenanceServer.registerRequestHandlers(
            "",
            &m_httpServer.httpMessageDispatcher());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);
    }

    void whenRequestMallocInfo()
    {
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kMallocInfo)));
    }

    void thenRequestSucceeded()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void andMallocInfoIsProvided()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);
        ASSERT_FALSE(msgBody->isEmpty());

        // TODO: #ak Add stronger validation.
    }

private:
    http::TestHttpServer m_httpServer;
    maintenance::Server m_maintenanceServer;
    http::HttpClient m_httpClient;

    nx::utils::Url requestUrl(const std::string& requestName) const
    {
        return url::Builder().setEndpoint(m_httpServer.serverAddress())
            .setScheme(http::kUrlSchemeName).setPath(kMaintenance)
            .appendPath(requestName).toUrl();
    }
};

TEST_F(MaintenanceServer, malloc_info)
{
    whenRequestMallocInfo();

    thenRequestSucceeded();
    andMallocInfoIsProvided();
}

} // namespace nx::network::maintenance::test
