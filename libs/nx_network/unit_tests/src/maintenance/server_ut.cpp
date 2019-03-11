#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::maintenance::test {

struct DebugCounters
{
    int tcpSocketCount = 0;
    int stunConnectionCount = 0;
    int httpConnectionCount = 0;
};

#define DebugCounters_Fields (tcpSocketCount)(stunConnectionCount)(httpConnectionCount)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DebugCounters),
    (json),
    _Fields)

//-------------------------------------------------------------------------------------------------

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

    void whenFetchDebugCounters()
    {
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kDebugCounters)));
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

    void andDebugCountersReceived()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);

        const auto debugCounters = QJson::deserialized<DebugCounters>(*msgBody);
        ASSERT_EQ(1U, debugCounters.httpConnectionCount);
        ASSERT_EQ(2U, debugCounters.tcpSocketCount);
        ASSERT_EQ(0U, debugCounters.stunConnectionCount);
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

TEST_F(MaintenanceServer, debug_counters)
{
    whenFetchDebugCounters();

    thenRequestSucceeded();
    andDebugCountersReceived();
}

} // namespace nx::network::maintenance::test
