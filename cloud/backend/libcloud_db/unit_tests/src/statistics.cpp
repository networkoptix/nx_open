#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/statistics/provider.h>

#include "functional_tests/test_setup.h"

namespace nx::cloud::db::test {

class Statistics:
    public CdbFunctionalTest
{
    using base_type = CdbFunctionalTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void fetchStatistics()
    {
        const auto url = network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(endpoint()).setPath(kStatisticsMetricsPath);

        network::http::HttpClient httpClient;
        httpClient.setResponseReadTimeout(network::kNoTimeout);

        ASSERT_TRUE(httpClient.doGet(url));
        ASSERT_NE(nullptr, httpClient.response());
        ASSERT_EQ(
            network::http::StatusCode::ok,
            httpClient.response()->statusLine.statusCode);

        auto body = httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(static_cast<bool>(body));

        ASSERT_TRUE(QJson::deserialize(*body, &m_response));
    }

    void assertStatisticsContainsExpectedData()
    {
        ASSERT_GT(m_response.http.connectionCount, 0);
    }

private:
    statistics::Statistics m_response;
};

TEST_F(Statistics, statistics_is_available_through_http_api)
{
    fetchStatistics();
    assertStatisticsContainsExpectedData();
}

} // namespace nx::cloud::db::test
