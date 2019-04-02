#pragma once

#include "params.h"

namespace nx::network::rest {

struct Response
{
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined;
    std::optional<Content> content;
    bool isUndefinedContentLength = false;
    nx::network::http::HttpHeaders httpHeaders;

    Response(
        nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined);

    static Response result(const QnJsonRestResult& result);

    template<typename T>
    static Response reply(const T& data);

    static Response error(const QnRestResult::ErrorDescriptor& descriptor);

    template<typename... Args>
    static Response error(QnRestResult::Error errorCode, Args... args);
};

//--------------------------------------------------------------------------------------------------

template<typename T>
Response Response::reply(const T& data)
{
    // TODO: Add support for other then JSON results?
    QnJsonRestResult json;
    json.setReply(data);
    return result(json);
}

template<typename... Args>
Response Response::error(QnRestResult::Error code, Args... args)
{
    return error({code, qStringListInit(std::forward<Args>(args)...)});
}

} // namespace nx::network::rest

