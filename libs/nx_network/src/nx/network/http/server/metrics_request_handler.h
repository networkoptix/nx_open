// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nx/prometheus/registry.h>

#include "abstract_http_request_handler.h"

namespace nx::network::http::server {

/**
 * Golden-signal metrics of an HTTP server, matching the conventions of the Go nxhttp package:
 * - http_server_request_duration_seconds histogram: latency, traffic (its _count) and errors
 *   (filtered by the http_response_status_code label);
 * - http_server_active_requests gauge: saturation.
 *
 * This class is transport-agnostic: request handling code reports plain method/route/status
 * values, wiring into a concrete HTTP server is up to the caller.
 */
class NX_NETWORK_API HttpRequestMetrics
{
private:
    struct SharedState;

public:
    // OpenTelemetry semantic-convention boundaries of http.server.request.duration.
    static inline const std::vector<double> kDefaultDurationBuckets =
        {0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.5, 5.0, 7.5, 10.0};
    static inline const std::string kLabelServerName = "http_server_name";

    explicit HttpRequestMetrics(
        nx::prometheus::Registry* registry,
        std::string serverName = "public",
        std::vector<double> durationBuckets = kDefaultDurationBuckets);

    /**
     * Measures one request: keeps http_server_active_requests incremented for its lifetime and,
     * on destruction, records the duration histogram sample. The route and status code are set
     * as they become known (the route template is only known after dispatch, and never for
     * unmatched or rejected requests). If no status code was set by destruction time (the
     * request produced no response), only the gauge is decremented - no duration sample.
     */
    class NX_NETWORK_API RequestRecorder
    {
    public:
        ~RequestRecorder();

        RequestRecorder(RequestRecorder&& other) noexcept;
        RequestRecorder(const RequestRecorder&) = delete;
        RequestRecorder& operator=(const RequestRecorder&) = delete;
        RequestRecorder& operator=(RequestRecorder&&) = delete;

        void setRoute(std::string routeTemplate);
        void setStatusCode(int statusCode);

    private:
        friend class HttpRequestMetrics;
        RequestRecorder(std::shared_ptr<const SharedState> state, std::string method);

        // Null means inert (moved-from). Needed to avoid duplicates during destruction.
        std::shared_ptr<const SharedState> m_state;
        std::string m_method;
        std::string m_route;
        std::optional<int> m_statusCode;
        std::chrono::steady_clock::time_point m_startTime;
    };

    [[nodiscard]] RequestRecorder newRequestStarted(std::string method);

private:
    // Immutable per-server metric handles, resolved once. Shared with every RequestRecorder so
    // a recorder can outlive its HttpRequestMetrics, and so no per-request copies are needed.
    struct SharedState
    {
        nx::prometheus::HistogramFamily durationFamily;
        nx::prometheus::Labels additionalLabels;
        nx::prometheus::Gauge activeRequests;
    };

    static std::shared_ptr<const SharedState> makeState(
        nx::prometheus::Registry* registry,
        std::string serverName,
        std::vector<double> durationBuckets);

    static void recordRequest(
        const SharedState& state,
        const std::string& method,
        const std::string& route,
        int statusCode,
        std::chrono::duration<double> duration);

    const std::shared_ptr<const SharedState> m_state;
};

/**
 * Records golden-signal Prometheus metrics for every request passing through: duration
 * histogram (labeled with method, route template and status code) and in-flight gauge.
 */
class NX_NETWORK_API MetricsRequestHandler: public IntermediaryHandler
{
public:
    MetricsRequestHandler(AbstractRequestHandler* nextHandler, HttpRequestMetrics* metrics);

    virtual void serve(
        RequestContext requestContext,
        nx::MoveOnlyFunc<void(RequestResult)> completionHandler) override;

private:
    HttpRequestMetrics* m_metrics;
};

} // namespace nx::network::http::server
