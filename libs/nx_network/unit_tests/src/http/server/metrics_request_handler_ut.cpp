// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include <nx/network/http/server/metrics_request_handler.h>
#include <nx/prometheus/registry.h>

namespace nx::network::http::server::test {

namespace {

class TestNextHandler: public AbstractRequestHandler
{
public:
    StatusCode::Value responseStatus = StatusCode::ok;
    std::string responsePathTemplate;
    bool dropCompletionHandler = false;
    std::optional<nx::MoveOnlyFunc<void(RequestResult)>> storedCompletionHandler;

    virtual void serve(
        RequestContext /*requestContext*/,
        nx::MoveOnlyFunc<void(RequestResult)> completionHandler) override
    {
        if (dropCompletionHandler)
            return; //< The MoveOnlyFunc is destroyed uninvoked, as if the client disconnected.

        storedCompletionHandler.emplace(std::move(completionHandler));
    }

    void completeStoredRequest()
    {
        RequestResult result(responseStatus);
        result.pathTemplate = responsePathTemplate;
        (*storedCompletionHandler)(std::move(result));
        storedCompletionHandler.reset(); //< Releases the recorder, as production code does.
    }
};

RequestContext prepareRequestContext(const Method& method, const std::string& path)
{
    Request request;
    request.requestLine.method = method;
    request.requestLine.version = http_1_1;
    request.requestLine.url = nx::Url("http://127.0.0.1:7001" + path);

    return RequestContext(
        ConnectionAttrs{},
        std::weak_ptr<HttpServerConnection>{},
        SocketAddress(),
        /*attrs*/ {},
        std::move(request));
}

} // namespace

class HttpServerMetricsRequestHandler: public ::testing::Test
{
protected:
    double activeRequests()
    {
        return registry
            .gauge(
                "http_server_active_requests",
                "Number of in-flight HTTP server requests",
                {{HttpRequestMetrics::kLabelServerName, kServerName}})
            .value();
    }

    static inline const std::string kServerName = "public";
    nx::prometheus::Registry registry{"testService", "dev"};
    HttpRequestMetrics metrics{&registry, kServerName};
    TestNextHandler nextHandler;
    MetricsRequestHandler metricsRequestHandler{&nextHandler, &metrics};
};

TEST_F(HttpServerMetricsRequestHandler, dropped_request_only_touches_the_in_flight_gauge)
{
    nextHandler.dropCompletionHandler = true;

    metricsRequestHandler.serve(
        prepareRequestContext(Method::get, "/ping"),
        [](RequestResult /*result*/)
        {
            FAIL() << "Completion must not be invoked";
        });

    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
    EXPECT_EQ(
        registry.serialize().find("http_server_request_duration_seconds_count{"),
        std::string::npos);
}

TEST_F(HttpServerMetricsRequestHandler, request_completing_after_metrics_destruction_is_safe)
{
    auto ownedMetrics = std::make_unique<HttpRequestMetrics>(&registry, kServerName);
    TestNextHandler ownedNextHandler;
    ownedNextHandler.responseStatus = StatusCode::ok;
    ownedNextHandler.responsePathTemplate = "/ping";
    MetricsRequestHandler ownedMetricsRequestHandler(&ownedNextHandler, ownedMetrics.get());

    std::optional<RequestResult> receivedResult;
    ownedMetricsRequestHandler.serve(
        prepareRequestContext(Method::get, "/ping"),
        [&receivedResult](RequestResult result)
        {
            receivedResult.emplace(std::move(result));
        });

    ownedMetrics.reset(); //< The recorder held by the stored completion handler must survive.

    ownedNextHandler.completeStoredRequest();

    ASSERT_TRUE(receivedResult.has_value());
    EXPECT_NE(registry.serialize().find("http_response_status_code=\"200\""), std::string::npos);
    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
}

class HttpRequestMetricsTest: public ::testing::Test
{
protected:
    double activeRequests()
    {
        return registry
            .gauge(
                "http_server_active_requests",
                "Number of in-flight HTTP server requests",
                {{HttpRequestMetrics::kLabelServerName, kServerName}})
            .value();
    }

    static inline const std::string kServerName = "public";
    nx::prometheus::Registry registry{"testService", "dev"};
    HttpRequestMetrics metrics{&registry, kServerName};
};

TEST_F(HttpRequestMetricsTest, request_recorder_tracks_in_flight_requests)
{
    {
        auto outer = metrics.newRequestStarted("GET");
        EXPECT_DOUBLE_EQ(activeRequests(), 1.0);
        {
            auto inner = metrics.newRequestStarted("POST");
            EXPECT_DOUBLE_EQ(activeRequests(), 2.0);
        }
        EXPECT_DOUBLE_EQ(activeRequests(), 1.0);
    }
    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
}

TEST_F(HttpRequestMetricsTest, moved_request_recorder_does_not_double_decrement)
{
    std::optional<HttpRequestMetrics::RequestRecorder> moved;
    {
        auto original = metrics.newRequestStarted("GET");
        moved.emplace(std::move(original));
        EXPECT_DOUBLE_EQ(activeRequests(), 1.0);
    }
    EXPECT_DOUBLE_EQ(activeRequests(), 1.0);

    moved->setStatusCode(200);
    moved.reset();
    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
    EXPECT_NE(registry.serialize().find("http_response_status_code=\"200\""), std::string::npos);
}

TEST_F(HttpRequestMetricsTest, request_recorder_without_status_only_touches_the_gauge)
{
    {
        // E.g. the client disconnected and the completion handler was dropped uninvoked.
        auto recorder = metrics.newRequestStarted("DELETE");
    }

    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
    // No response was produced, so no duration sample must be recorded.
    EXPECT_EQ(registry.serialize().find("http_request_method=\"DELETE\""), std::string::npos);
}

TEST_F(HttpRequestMetricsTest, request_recorder_without_route_records_empty_route_label)
{
    {
        // E.g. a 404 for an unmatched path: no route template exists.
        auto recorder = metrics.newRequestStarted("GET");
        recorder.setStatusCode(404);
    }

    const std::string serialized = registry.serialize();
    EXPECT_NE(serialized.find("http_route=\"\""), std::string::npos);
    EXPECT_NE(serialized.find("http_response_status_code=\"404\""), std::string::npos);
}

TEST_F(HttpRequestMetricsTest, recorder_outlives_its_metrics_object)
{
    std::optional<HttpRequestMetrics::RequestRecorder> recorder;
    {
        HttpRequestMetrics innerMetrics(&registry, kServerName);
        recorder.emplace(innerMetrics.newRequestStarted("GET"));
    } //< innerMetrics is destroyed here; the recorder must remain usable.

    recorder->setRoute("/account/{accountId}");
    recorder->setStatusCode(200);
    recorder.reset();

    const std::string serialized = registry.serialize();
    EXPECT_NE(serialized.find("http_response_status_code=\"200\""), std::string::npos);
    EXPECT_DOUBLE_EQ(activeRequests(), 0.0);
}

TEST_F(HttpRequestMetricsTest, unknown_method_is_normalized_to_other)
{
    {
        auto recorder = metrics.newRequestStarted("FOO");
        recorder.setStatusCode(200);
    }

    const std::string serialized = registry.serialize();
    EXPECT_NE(serialized.find("http_request_method=\"_OTHER\""), std::string::npos);
    EXPECT_EQ(serialized.find("http_request_method=\"FOO\""), std::string::npos);

    {
        auto recorder = metrics.newRequestStarted("gEt");
        recorder.setRoute("/ping");
        recorder.setStatusCode(200);
    }

    // Normalization is case-sensitive: "gEt" is not the known "GET".
    EXPECT_EQ(registry.serialize().find("http_request_method=\"gEt\""), std::string::npos);
}

TEST_F(HttpRequestMetricsTest, known_method_is_passed_through)
{
    {
        auto recorder = metrics.newRequestStarted("GET");
        recorder.setRoute("/ping");
        recorder.setStatusCode(200);
    }

    EXPECT_NE(registry.serialize().find("http_request_method=\"GET\""), std::string::npos);
}

} // namespace nx::network::http::server::test
