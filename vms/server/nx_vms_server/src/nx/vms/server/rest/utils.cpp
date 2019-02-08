#include "utils.h"

namespace nx::vms::server::rest {

nx::network::http::StatusCode::Value errorToHttpStatusCode(QnRestResult::Error error)
{
    using namespace nx::network;
    switch (error)
    {
        case QnRestResult::Error::NoError:
            return http::StatusCode::ok;
        case QnRestResult::Error::InvalidParameter:
        case QnRestResult::Error::MissingParameter:
        case QnRestResult::Error::CantProcessRequest:
            return http::StatusCode::unprocessableEntity;
        case QnRestResult::Error::Forbidden:
            return http::StatusCode::forbidden;
        default:
            return http::StatusCode::badRequest;
    }
}

JsonRestResponse makeResponse(QnRestResult::Error error, const QString& errorString)
{
    JsonRestResponse response;
    response.statusCode = errorToHttpStatusCode(error);
    response.json.setError(error, errorString);

    return response;
}

JsonRestResponse makeResponse(QnRestResult::Error error, const QStringList& arguments)
{
    JsonRestResponse response;
    response.statusCode = errorToHttpStatusCode(error);
    response.json.setError(QnRestResult::ErrorDescriptor(error, arguments));

    return response;
}

} // namespace nx::vms::server::rest
