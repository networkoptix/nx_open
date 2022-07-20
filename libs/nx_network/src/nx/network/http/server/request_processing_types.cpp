// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_processing_types.h"

namespace nx::network::http {

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
