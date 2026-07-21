// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/metrics_request_handler.h>
#include <nx/network/http/server/private_http_server.h>
#include <nx/prometheus/registry.h>

namespace nx::network::http::server::test {

namespace {

std::optional<double> requestCountForServer(
    const std::string& scraped,
    const std::string& serverName)
{
    const std::string serverNameLabel =
        HttpRequestMetrics::kLabelServerName + "=\"" + serverName + "\"";

    std::istringstream stream(scraped);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.rfind("http_server_request_duration_seconds_count", 0) == 0
            && line.find(serverNameLabel) != std::string::npos)
        {
            return std::stod(line.substr(line.rfind(' ') + 1));
        }
    }
    return std::nullopt;
}

} // namespace

class PrivateHttpServerTest: public ::testing::Test
{
protected:
    std::string scrape()
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);
        const auto port = m_server.endpoints().front().port;
        EXPECT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1/metrics").args(port))));
        EXPECT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
        const auto body = client.fetchEntireMessageBody();
        return body ? body->toStdString() : std::string();
    }

    nx::prometheus::Registry m_registry{"testService", "dev"};
    PrivateHttpServer m_server{&m_registry, SocketAddress("127.0.0.1:0")};
};

TEST_F(PrivateHttpServerTest, scrape_exposes_public_server_request_count)
{
    // The production topology: a public-facing server and the private server share one metrics
    // registry, and the public server's traffic is observed by scraping the private /metrics.
    Settings publicSettings;
    publicSettings.endpoints.push_back(SocketAddress("127.0.0.1:0"));

    rest::MessageDispatcher publicDispatcher;
    publicDispatcher.registerRequestProcessorFunc(
        Method::get,
        "/ping",
        [](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
        {
            completionHandler(StatusCode::ok);
        });

    auto publicServer = Builder::buildOrThrow(
        publicSettings,
        &publicDispatcher,
        &m_registry,
        /*serverName*/ "public");
    ASSERT_TRUE(publicServer->listen());
    ASSERT_TRUE(m_server.listen());

    // A single request to the public server.
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);
        const auto port = publicServer->endpoints().front().port;
        ASSERT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1/ping").args(port))));
        ASSERT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
    }

    // The duration histogram (whose _count is the request total) is recorded server-side once
    // the request completes, which may lag the client's receipt of the response. Scrape until
    // the public server's count series appears.
    std::optional<double> count;
    for (int i = 0; i < 100; ++i)
    {
        count = requestCountForServer(scrape(), /*serverName*/ "public");
        if (count == 1.0)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_EQ(count, 1.0);
}

} // namespace nx::network::http::server::test
