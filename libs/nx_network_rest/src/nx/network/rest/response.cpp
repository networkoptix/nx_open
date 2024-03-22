// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "response.h"

namespace nx::network::rest {

Response::Response(http::StatusCode::Value statusCode, http::HttpHeaders httpHeaders):
    statusCode(statusCode), httpHeaders(std::move(httpHeaders))
{
}

Response::Response(const Result& restResult, Qn::SerializationFormat format)
{
    switch (format)
    {
        case Qn::SerializationFormat::json:
            *this = result(JsonResult(restResult));
            return;

        case Qn::SerializationFormat::ubjson:
            *this = result(UbjsonResult(restResult));
            return;

        default:
            NX_ASSERT(false, "Unsupported serialization format %1", format);
            statusCode = http::StatusCode::internalServerError;
    }
}

void Response::insertOrReplaceCorsHeaders(
    const nx::network::http::Request& request,
    const QString& supportedOrigins,
    std::string_view methods)
{
    nx::network::http::insertOrReplaceCorsHeaders(
        &httpHeaders,
        request.requestLine.method,
        nx::network::http::getHeaderValue(request.headers, "Origin"),
        supportedOrigins.toStdString(),
        std::move(methods));
}

Response Response::result(const JsonResult& jsonResult)
{
    Response response;
    response.statusCode = Result::toHttpStatus(jsonResult.error);
    response.content = Content{
        http::header::ContentType::kJson,
        QJson::serialized(jsonResult)};
    return response;
}

Response Response::result(const UbjsonResult& ubjsonResult)
{
    Response response;
    response.statusCode = Result::toHttpStatus(ubjsonResult.error);
    response.content = Content{
        http::header::ContentType::kUbjson,
        QnUbjson::serialized(ubjsonResult)};

    return response;
}

} // namespace nx::network::rest
