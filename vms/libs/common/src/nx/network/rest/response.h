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
        const QnRestResult& result,
        Qn::SerializationFormat format = Qn::JsonFormat);

    static Response result(const QnJsonRestResult& jsonResult);
    static Response result(const QnUbjsonRestResult& ubjsonResult);

    template<typename ResultType = QnJsonRestResult, typename DataType>
    static Response reply(const DataType& data);

    template<typename DataType>
    static Response reply(const DataType& data, Qn::SerializationFormat format);

    static Response error(const QnRestResult::ErrorDescriptor& descriptor);

    template<typename... Args>
    static Response error(QnRestResult::Error code, Args... args);

    template<typename... Args>
    static Response error(http::StatusCode::Value status, QnRestResult::Error code, Args... args);
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
        case Qn::JsonFormat: return reply<QnJsonRestResult>(data);
        case Qn::UbjsonFormat: return reply<QnJsonRestResult>(data);
        default: break;
    }

    NX_ASSERT(false, lm("Unsupported serialization format %1").arg(QnLexical::serialized(format)));
    return {http::StatusCode::internalServerError};
}

template<typename... Args>
Response Response::error(QnRestResult::Error code, Args... args)
{
    return error({code, qStringListInit(std::forward<Args>(args)...)});
}

template<typename... Args>
Response Response::error(http::StatusCode::Value status, QnRestResult::Error code, Args... args)
{
    auto response = error({code, qStringListInit(std::forward<Args>(args)...)});
    response.statusCode = status;
    return response;
}

} // namespace nx::network::rest

