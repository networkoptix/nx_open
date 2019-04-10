#include "response.h"

namespace nx::network::rest {

Response::Response(http::StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

Response Response::result(const Result& restResult, Qn::SerializationFormat format)
{
    switch (format)
    {
        case Qn::JsonFormat: return result(JsonResult(restResult));
        case Qn::UbjsonFormat: return result(UbjsonResult(restResult));
        default: break;
    }

    NX_ASSERT(false, lm("Unsupported serialization format %1").arg(QnLexical::serialized(format)));
    return {http::StatusCode::internalServerError};
}

Response Response::result(const JsonResult& jsonResult)
{
    Response response;
    response.statusCode = Result::toHttpStatus(jsonResult.error);
    response.content = Content{kJsonContentType, QJson::serialized(jsonResult)};
    return response;
}

Response Response::result(const UbjsonResult& ubjsonResult)
{
    Response response;
    response.statusCode = Result::toHttpStatus(ubjsonResult.error);
    response.content = Content{kUbjsonContentType, QnUbjson::serialized(ubjsonResult)};
    return response;
}

Response Response::error(const Result::ErrorDescriptor& descriptor)
{
    Result rest;
    rest.setError(descriptor);
    return result(rest);
}

} // namespace nx::network::rest
