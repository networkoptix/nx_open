// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_processing_types.h"

#include <nx/utils/string.h>

namespace nx::network::http {

static std::optional<TraceContext> parseAmznTraceId(const std::string& text)
{
    // format: Root=1-5e7b1b1d-4b8b1b1d1;Self=1-5e7b1b1d-4b8b3b1d9.

    TraceContext ctx;
    nx::utils::splitNameValuePairs(
        text, ';', '=',
        [&ctx](const std::string_view name, const std::string_view value)
        {
            if (name == "Root")
            {
                const auto& [tokens, count] = nx::utils::split_n<3>(value, '-');
                if (count >= 3)
                    ctx.traceId = tokens[2];
            }
        });

    ctx.header = {"X-Amzn-Trace-Id", text};
    return ctx;
}

static std::optional<TraceContext> parseGcpTraceContext(const std::string& text)
{
    // format: TRACE_ID[/SPAN_ID[;o=OPTIONS]]

    TraceContext ctx;
    const auto& [tokens, count] = nx::utils::split_n<2>(text, '/');
    ctx.traceId = tokens[0];

    ctx.header = {"X-Cloud-Trace-Context", text};
    return ctx;
}

static std::optional<TraceContext> parseNxRequestId(const std::string& text)
{
    return TraceContext{.traceId = text, .header = {"X-Request-Id", text}};
}

std::optional<TraceContext> parseTraceContext(const HttpHeaders& headers)
{
    if (auto it = headers.find("X-Amzn-Trace-Id"); it != headers.end())
        return parseAmznTraceId(it->second);

    if (auto it = headers.find("X-Cloud-Trace-Context"); it != headers.end())
        return parseGcpTraceContext(it->second);

    if (auto it = headers.find("X-Request-Id"); it != headers.end())
        return parseNxRequestId(it->second);

    return std::nullopt; // No trace context headers found.
}

//-------------------------------------------------------------------------------------------------

RequestContext::RequestContext() = default;

RequestContext::RequestContext(
    ConnectionAttrs connectionAttrs,
    std::weak_ptr<http::HttpServerConnection> connection,
    const SocketAddress& clientEndpoint,
    Attrs attrs,
    http::Request request,
    std::unique_ptr<AbstractMsgBodySourceWithCache> body)
    :
    connectionAttrs(std::move(connectionAttrs)),
    conn(connection),
    clientEndpoint(clientEndpoint),
    attrs(std::move(attrs)),
    request(std::move(request)),
    body(std::move(body)),
    traceContext(parseTraceContext(this->request.headers))
{
}

std::string RequestContext::traceId() const
{
    return traceContext ? traceContext->traceId : std::string();
}

//-------------------------------------------------------------------------------------------------

RequestResult::RequestResult(StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

RequestResult::RequestResult(
    nx::network::http::StatusCode::Value statusCode,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
    :
    statusCode(statusCode),
    body(std::move(body))
{
}

RequestResult::RequestResult(
    nx::network::http::StatusCode::Value statusCode,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body,
    ConnectionEvents connectionEvents)
    :
    statusCode(statusCode),
    body(std::move(body)),
    connectionEvents(std::move(connectionEvents))
{
}

RequestResult::RequestResult(
    StatusCode::Value statusCode,
    nx::network::http::HttpHeaders responseHeaders,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody)
    :
    statusCode(statusCode),
    headers(std::move(responseHeaders)),
    body(std::move(msgBody))
{
}

} // namespace nx::network::http
