#pragma once

#include "params.h"

namespace nx::network::rest {

struct Response
{
    http::StatusCode::Value statusCode = http::StatusCode::undefined;
    std::optional<Content> content;
    bool isUndefinedContentLength = false;
    nx::network::http::HttpHeaders httpHeaders;

    Response(http::StatusCode::Value statusCode = http::StatusCode::undefined);

    static Response result(
        const Result& result,
        Qn::SerializationFormat format = Qn::JsonFormat);

    static Response result(const JsonResult& jsonResult);
    static Response result(const UbjsonResult& ubjsonResult);

    template<typename ResultType = JsonResult, typename DataType>
    static Response reply(const DataType& data);

    template<typename DataType>
    static Response reply(const DataType& data, Qn::SerializationFormat format);

    static Response error(const Result::ErrorDescriptor& descriptor);

    template<typename... Args>
    static Response error(Result::Error code, Args... args);

    template<typename... Args>
    static Response error(http::StatusCode::Value status, Result::Error code, Args... args);
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
        case Qn::JsonFormat: return reply<JsonResult>(data);
        case Qn::UbjsonFormat: return reply<UbjsonResult>(data);
        default: break;
    }

    NX_ASSERT(false, lm("Unsupported serialization format %1").arg(QnLexical::serialized(format)));
    return {http::StatusCode::internalServerError};
}

template<typename... Args>
Response Response::error(Result::Error code, Args... args)
{
    return error({code, std::forward<Args>(args)...});
}

template<typename... Args>
Response Response::error(http::StatusCode::Value status, Result::Error code, Args... args)
{
    auto response = error({code, std::forward<Args>(args)...});
    response.statusCode = status;
    return response;
}

} // namespace nx::network::rest

