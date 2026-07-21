// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metrics_request_handler.h"

#include <utility>

#include <nx/network/http/http_types.h>

namespace nx::network::http::server {

namespace {

constexpr char kRequestDurationName[] = "http_server_request_duration_seconds";
constexpr char kRequestDurationHelp[] = "Duration of HTTP server requests";
constexpr char kActiveRequestsName[] = "http_server_active_requests";
constexpr char kActiveRequestsHelp[] = "Number of in-flight HTTP server requests";

const std::string kOtherMethodLabel = "_OTHER";

const std::string& normalizedMethod(const std::string& method)
{
    return Method::isKnown(method) ? method : kOtherMethodLabel;
}

} // namespace

HttpRequestMetrics::HttpRequestMetrics(
    nx::prometheus::Registry* registry,
    std::string serverName,
    std::vector<double> durationBuckets):
    m_state(makeState(registry, std::move(serverName), std::move(durationBuckets)))
{
}

std::shared_ptr<const HttpRequestMetrics::SharedState> HttpRequestMetrics::makeState(
    nx::prometheus::Registry* registry,
    std::string serverName,
    std::vector<double> durationBuckets)
{
    nx::prometheus::Labels additionalLabels = serverName.empty()
        ? nx::prometheus::Labels{}
        : nx::prometheus::Labels{{kLabelServerName, std::move(serverName)}};

    return std::make_shared<const SharedState>(SharedState{
        registry->histogramFamily(
            kRequestDurationName,
            kRequestDurationHelp,
            std::move(durationBuckets)),
        additionalLabels,
        registry->gauge(kActiveRequestsName, kActiveRequestsHelp, additionalLabels)});
}

HttpRequestMetrics::RequestRecorder HttpRequestMetrics::newRequestStarted(std::string method)
{
    return RequestRecorder(m_state, std::move(method));
}

void HttpRequestMetrics::recordRequest(
    const SharedState& state,
    const std::string& method,
    const std::string& route,
    int statusCode,
    std::chrono::duration<double> duration)
{
    nx::prometheus::Labels labels = state.additionalLabels;
    labels.emplace("http_request_method", normalizedMethod(method));
    labels.emplace("http_route", route);
    labels.emplace("http_response_status_code", std::to_string(statusCode));

    state.durationFamily.withLabels(labels).observe(duration);
}

HttpRequestMetrics::RequestRecorder::RequestRecorder(
    std::shared_ptr<const SharedState> state,
    std::string method):
    m_state(std::move(state)),
    m_method(std::move(method)),
    m_startTime(std::chrono::steady_clock::now())
{
    auto activeRequests = m_state->activeRequests;
    activeRequests.increment();
}

HttpRequestMetrics::RequestRecorder::~RequestRecorder()
{
    if (!m_state)
        return;

    auto activeRequests = m_state->activeRequests;
    activeRequests.decrement();
    if (m_statusCode)
    {
        HttpRequestMetrics::recordRequest(
            *m_state,
            m_method,
            m_route,
            *m_statusCode,
            std::chrono::steady_clock::now() - m_startTime);
    }
}

HttpRequestMetrics::RequestRecorder::RequestRecorder(RequestRecorder&& other) noexcept:
    m_state(std::move(other.m_state)),
    m_method(std::move(other.m_method)),
    m_route(std::move(other.m_route)),
    m_statusCode(other.m_statusCode),
    m_startTime(other.m_startTime)
{
}

void HttpRequestMetrics::RequestRecorder::setRoute(std::string routeTemplate)
{
    m_route = std::move(routeTemplate);
}

void HttpRequestMetrics::RequestRecorder::setStatusCode(int statusCode)
{
    m_statusCode = statusCode;
}

MetricsRequestHandler::MetricsRequestHandler(
    AbstractRequestHandler* nextHandler,
    HttpRequestMetrics* metrics):
    IntermediaryHandler(nextHandler),
    m_metrics(metrics)
{
}

void MetricsRequestHandler::serve(
    RequestContext requestContext,
    nx::MoveOnlyFunc<void(RequestResult)> completionHandler)
{
    auto recorder =
        m_metrics->newRequestStarted(requestContext.request.requestLine.method.toString());

    nextHandler()->serve(
        std::move(requestContext),
        [recorder = std::move(recorder),
         completionHandler = std::move(completionHandler)](RequestResult result) mutable
        {
            recorder.setRoute(result.pathTemplate);
            recorder.setStatusCode(result.statusCode);
            completionHandler(std::move(result));
        });
}

} // namespace nx::network::http::server
