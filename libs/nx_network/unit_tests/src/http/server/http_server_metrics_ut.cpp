// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/metrics_request_handler.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/prometheus/registry.h>

namespace nx::network::http::server::test {

namespace {

constexpr char kServiceName[] = "testService";
constexpr char kEnvironment[] = "dev";
constexpr char kServerName[] = "e2e";

} // namespace

class HttpServerGoldenSignalMetrics: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_settings.endpoints.push_back(SocketAddress("127.0.0.1:0"));

        m_dispatcher.registerRequestProcessorFunc(
            Method::get,
            "/account/{accountId}",
            [](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
            {
                completionHandler(StatusCode::ok);
            });

        // Drives the Errors golden signal: a non-2xx status so the duration histogram
        // carries a 5xx series.
        m_dispatcher.registerRequestProcessorFunc(
            Method::get,
            "/boom",
            [](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
            {
                completionHandler(StatusCode::internalServerError);
            });

        m_server = Builder::buildOrThrow(
            m_settings,
            &m_dispatcher,
            &m_registry,
            /*serverName*/ kServerName);
        ASSERT_TRUE(m_server->listen());
    }

    void whenMakeRequest(const std::string& path)
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);
        const auto port = m_server->endpoints().front().port;
        ASSERT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1%2").args(port, path))));
    }

    double activeRequests()
    {
        return m_registry
            .gauge(
                "http_server_active_requests",
                "Number of in-flight HTTP server requests",
                {{HttpRequestMetrics::kLabelServerName, kServerName}})
            .value();
    }

    nx::prometheus::Registry m_registry{kServiceName, kEnvironment};
    rest::MessageDispatcher m_dispatcher;
    Settings m_settings;
    std::unique_ptr<MultiEndpointServer> m_server;
};

TEST_F(HttpServerGoldenSignalMetrics, all_four_golden_signals_are_exposed)
{
    whenMakeRequest("/account/id1"); //< 200.
    whenMakeRequest("/boom"); //< 500.
    whenMakeRequest("/missing"); //< 404, never reaches a registered handler.

    const std::string serialized = m_registry.serialize();

    // Latency: request duration histogram with buckets.
    EXPECT_NE(
        serialized.find("# TYPE http_server_request_duration_seconds histogram"),
        std::string::npos);
    EXPECT_NE(serialized.find("le=\"+Inf\""), std::string::npos);

    // Traffic: request rate source is the histogram _count.
    EXPECT_NE(serialized.find("http_server_request_duration_seconds_count{"), std::string::npos);

    // Errors: the error rate is derivable by filtering on the status-code label.
    EXPECT_NE(serialized.find("http_response_status_code=\"500\""), std::string::npos);

    // Saturation: the in-flight gauge, back to zero after the requests completed.
    EXPECT_NE(serialized.find("# TYPE http_server_active_requests gauge"), std::string::npos);
    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);

    // The route label is the low-cardinality template, not the concrete path.
    EXPECT_NE(serialized.find("http_route=\"/account/{accountId}\""), std::string::npos);
    EXPECT_EQ(serialized.find("/account/id1"), std::string::npos);

    // Unmatched requests are still counted, with an empty route.
    EXPECT_NE(serialized.find("http_response_status_code=\"404\""), std::string::npos);
    EXPECT_NE(serialized.find("http_route=\"\""), std::string::npos);

    // Every series carries the resource and server-identity labels.
    EXPECT_NE(
        serialized.find(nx::prometheus::Registry::kLabelServiceName + "=\"" + kServiceName + "\""),
        std::string::npos);
    EXPECT_NE(
        serialized.find(nx::prometheus::Registry::kLabelEnvironment + "=\"" + kEnvironment + "\""),
        std::string::npos);
    EXPECT_NE(
        serialized.find(HttpRequestMetrics::kLabelServerName + "=\"" + kServerName + "\""),
        std::string::npos);
}

} // namespace nx::network::http::server::test
