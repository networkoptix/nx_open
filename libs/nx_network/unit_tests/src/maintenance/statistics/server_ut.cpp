// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>

#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/statistics/request_path.h>
#include <nx/network/maintenance/statistics/statistics.h>
#include <nx/network/maintenance/statistics/server.h>

namespace nx::network::maintenance::statistics::test {

class StatisticsServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_statisticsServer.registerRequestHandlers(
            kStatistics,
            &m_httpServer.httpMessageDispatcher());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);
    }

    void whenRequestStatistics()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kGeneral)));
    }

    void thenRequestSucceeded()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void andStatisticsAreProvided()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);
        ASSERT_FALSE(msgBody->empty());

        const auto [statistics, err] =
            nx::reflect::json::deserialize<statistics::Statistics>(*msgBody);
        ASSERT_TRUE(statistics.uptimeMsec >= std::chrono::milliseconds(1));

        NX_DEBUG(this, "serialized: %1", *msgBody);
        NX_DEBUG(this, "server uptime: %1", statistics.uptimeMsec);
    }

private:
    http::TestHttpServer m_httpServer;
    statistics::Server m_statisticsServer;
    http::HttpClient m_httpClient{ssl::kAcceptAnyCertificate};

    nx::utils::Url requestUrl(const std::string& requestName = std::string()) const
    {
        return url::Builder().setEndpoint(m_httpServer.serverAddress())
            .setScheme(http::kUrlSchemeName).setPath(kStatistics)
            .appendPath(requestName).toUrl();
    }
};

TEST_F(StatisticsServer, general_statistics)
{
    whenRequestStatistics();

    thenRequestSucceeded();

    andStatisticsAreProvided();
}

} // namespace nx::network::maintenance::statistics::test
