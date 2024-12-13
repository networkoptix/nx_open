// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "params.h"
#include "result.h"

namespace nx::network::rest {

struct NX_NETWORK_REST_API Response
{
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined;
    std::optional<Content> content;
    bool isUndefinedContentLength = false;
    nx::network::http::HttpHeaders httpHeaders;
    std::variant<rapidjson::Document, QJsonValue> contentBodyJson;

    Response(
        nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined,
        nx::network::http::HttpHeaders httpHeaders = {});

    Response(const Result& result, Qn::SerializationFormat format = Qn::SerializationFormat::json);

    void insertOrReplaceCorsHeaders(
        const nx::network::http::Request& request,
        const QString& supportedOrigins,
        std::string_view methods);

    static Response result(const JsonResult& jsonResult);
    static Response result(const UbjsonResult& ubjsonResult);

    template<typename ResultType = JsonResult, typename DataType>
    static Response reply(const DataType& data);

    template<typename DataType>
    static Response reply(const DataType& data, Qn::SerializationFormat format);
};

//--------------------------------------------------------------------------------------------------

template<typename ResultType, typename DataType>
Response Response::reply(const DataType& data)
{
    ResultType resultData;
    resultData.setReply(data);
    return result(resultData);
}

template<typename DataType>
Response Response::reply(const DataType& data, Qn::SerializationFormat format)
{
    switch (format)
    {
        case Qn::SerializationFormat::json: return reply<JsonResult>(data);
        case Qn::SerializationFormat::ubjson: return reply<UbjsonResult>(data);
        default: break;
    }

    NX_ASSERT(false, nx::format("Unsupported serialization format %1").arg(QnLexical::serialized(format)));
    return {nx::network::http::StatusCode::internalServerError};
}

} // namespace nx::network::rest
