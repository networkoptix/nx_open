#include "response.h"

namespace nx::network::rest {

Response::Response(http::StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

Response Response::result(const QnRestResult& restResult, Qn::SerializationFormat format)
{
    switch (format)
    {
        case Qn::JsonFormat: return result(QnJsonRestResult(restResult));
        case Qn::UbjsonFormat: return result(QnUbjsonRestResult(restResult));
        default: break;
    }

    NX_ASSERT(false, lm("Unsupported serrialization format %1").arg(QnLexical::serialized(format)));
    return {http::StatusCode::internalServerError};
}

Response Response::result(const QnJsonRestResult& jsonResult)
{
    Response response;
    response.statusCode = QnRestResult::toHttpStatus(jsonResult.error);
    response.content = Content{kJsonContentType, QJson::serialized(jsonResult)};
    return response;
}

Response Response::result(const QnUbjsonRestResult& ubjsonResult)
{
    Response response;
    response.statusCode = QnRestResult::toHttpStatus(ubjsonResult.error);
    response.content = Content{kUbjsonContentType, QnUbjson::serialized(ubjsonResult)};
    return response;
}

Response Response::error(const QnRestResult::ErrorDescriptor& descriptor)
{
    QnRestResult rest;
    rest.setError(descriptor);
    return result(rest);
}

} // namespace nx::network::rest
