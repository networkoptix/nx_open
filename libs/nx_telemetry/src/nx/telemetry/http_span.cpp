// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_span.h"

#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/context.h>

#include "private/span_data.h"

using namespace opentelemetry;

namespace {

class HttpHeaderWriter: public context::propagation::TextMapCarrier
{
    HttpHeaders& m_headers;

public:
    HttpHeaderWriter(HttpHeaders& headers): m_headers(headers) {}

    nostd::string_view Get(nostd::string_view /*key*/) const noexcept override
    {
        return {};
    }

    void Set(nostd::string_view key, nostd::string_view value) noexcept override
    {
        m_headers.emplace(std::string(key), std::string(value));
    }
};

class HttpHeaderReader: public context::propagation::TextMapCarrier
{
    const HttpHeaders& m_headers;

public:
    HttpHeaderReader(const HttpHeaders& headers): m_headers(headers) {}

    nostd::string_view Get(nostd::string_view key) const noexcept override
    {
        auto it = m_headers.find(std::string(key));
        if (it != m_headers.end())
            return nostd::string_view(it->second);
        return "";
    }

    void Set(nostd::string_view /*key*/, nostd::string_view /*value*/) noexcept override {}
};

} // namespace

namespace nx::telemetry {

HttpSpan::HttpSpan(const Span& span):
    Span(span)
{
}

void HttpSpan::setStatusCode(int httpStatusCode)
{
    setAttribute("http.status_code", httpStatusCode);
    setStatus(httpStatusCode < 400 ? Status::ok : Status::error);
}

void HttpSpan::setRoute(const std::string_view& route)
{
    if (!isValid())
        return;

    setAttribute("http.route", route);
    updateName(route);
}

HttpSpan HttpSpan::clientSpan(
    const std::string_view& method, const std::string_view& url, HttpHeaders& headers)
{
    HttpSpan span(url, Kind::client);
    if (!NX_ASSERT(span.isValid()))
        return span;

    span.setAttribute("http.method", method);
    span.setAttribute("http.url", url);

    auto spanScope = span.activate();

    HttpHeaderWriter headerWriter(headers);
    opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator()->Inject(
        headerWriter, opentelemetry::context::RuntimeContext::GetCurrent());

    return span;
}

HttpSpan HttpSpan::serverSpan(
    const std::string_view& method,
    const std::string_view& route,
    const std::string_view& url,
    const HttpHeaders& headers)
{
    const Span parentSpan = extractSpanFromHeaders(headers);
    HttpSpan span(route, parentSpan, Kind::server);

    span.setAttribute("http.method", method);
    span.setAttribute("http.route", route);
    span.setAttribute("http.url", url);

    return span;
}

Span HttpSpan::extractSpanFromHeaders(const HttpHeaders& headers)
{
    auto currentContext = opentelemetry::context::RuntimeContext::GetCurrent();
    auto extractedContext =
        opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator()
            ->Extract(HttpHeaderReader(headers), currentContext);

    auto parentOpenTelemetrySpan = trace::GetSpan(extractedContext);
    if (!parentOpenTelemetrySpan->GetContext().IsValid())
        return {};

    return HttpSpan(std::make_shared<SpanData>(parentOpenTelemetrySpan, /*reference*/ true));
}

} // namespace nx::telemetry
