#include "response.h"

namespace nx::network::rest {

Response::Response(nx::network::http::StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

Response Response::result(const QnJsonRestResult& result)
{
    Response response;
    response.statusCode = QnRestResult::toHttpStatus(result.error);
    response.content = Content{kJsonContentType, QJson::serialized(result)};
    return response;
}

Response Response::error(const QnRestResult::ErrorDescriptor& descriptor)
{
    QnRestResult rest;
    rest.setError(descriptor);
    return result(rest);
}

} // namespace nx::network::rest
