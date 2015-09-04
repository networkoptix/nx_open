/**********************************************************
* Sep 4, 2015
* akolesnikov
***********************************************************/

#include "types.h"


namespace nx {
namespace cdb {
namespace api {


nx_http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx_http::StatusCode::ok;
        case ResultCode::notAuthorized:
            return nx_http::StatusCode::unauthorized;
        case ResultCode::forbidden:
            return nx_http::StatusCode::forbidden;
        case ResultCode::notFound:
            return nx_http::StatusCode::notFound;
        case ResultCode::alreadyExists:
            return nx_http::StatusCode::forbidden;
        case ResultCode::dbError:
            return nx_http::StatusCode::internalServerError;
    }
    return nx_http::StatusCode::internalServerError;
}

ResultCode httpStatusCodeToResultCode(nx_http::StatusCode::Value statusCode)
{
    switch (statusCode)
    {
        case nx_http::StatusCode::ok:
            return ResultCode::ok;
        case nx_http::StatusCode::unauthorized:
            return ResultCode::notAuthorized;
        case nx_http::StatusCode::forbidden:
            return ResultCode::forbidden;
        case nx_http::StatusCode::notFound:
            return ResultCode::notFound;
        case nx_http::StatusCode::internalServerError:
            return ResultCode::dbError;
        case nx_http::StatusCode::notImplemented:
            return ResultCode::notImplemented;
        default:
            return ResultCode::unknownError;
    }
}


}   //api
}   //cdb
}   //nx
