/**********************************************************
* Sep 4, 2015
* akolesnikov
***********************************************************/

#include "types.h"

#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {
namespace api {

//TODO #ak too many different conversion functions here

nx_http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx_http::StatusCode::ok;
        case ResultCode::notAuthorized:
        case ResultCode::credentialsRemovedPermanently:
            return nx_http::StatusCode::unauthorized;
        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
            return nx_http::StatusCode::forbidden;
        case ResultCode::notFound:
            return nx_http::StatusCode::notFound;
        case ResultCode::alreadyExists:
            return nx_http::StatusCode::forbidden;
        case ResultCode::dbError:
            return nx_http::StatusCode::internalServerError;
        case ResultCode::networkError:
            return nx_http::StatusCode::internalServerError;
        case ResultCode::notImplemented:
            return nx_http::StatusCode::notImplemented;
        case ResultCode::unknownRealm:
        case ResultCode::invalidNonce:
            return nx_http::StatusCode::forbidden;
        case ResultCode::badUsername:
        case ResultCode::badRequest:
            return nx_http::StatusCode::badRequest;
        case ResultCode::serviceUnavailable:
            return nx_http::StatusCode::serviceUnavailable;
        case ResultCode::invalidFormat:
            return nx_http::StatusCode::badRequest;
        case ResultCode::unknownError:
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
        case nx_http::StatusCode::serviceUnavailable:
            return ResultCode::serviceUnavailable;
        default:
            return ResultCode::unknownError;
    }
}


nx_http::FusionRequestResult resultCodeToFusionRequestResult(ResultCode resultCode)
{
    if (resultCode == ResultCode::ok)
        return nx_http::FusionRequestResult();

    //setting correct error class
    nx_http::FusionRequestErrorClass requestResultCode = nx_http::FusionRequestErrorClass::noError;
    switch (resultCode)
    {
        case ResultCode::notAuthorized:
        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
        case ResultCode::badUsername:
        case ResultCode::badRequest:
        case ResultCode::invalidNonce:
        case ResultCode::unknownRealm:
        case ResultCode::credentialsRemovedPermanently:
            requestResultCode = nx_http::FusionRequestErrorClass::unauthorized;
            break;

        case ResultCode::notFound:
        case ResultCode::alreadyExists:
            requestResultCode = nx_http::FusionRequestErrorClass::logicError;
            break;

        case ResultCode::dbError:
        case ResultCode::networkError:
        case ResultCode::serviceUnavailable:
            requestResultCode = nx_http::FusionRequestErrorClass::ioError;
            break;

        default:
            requestResultCode = nx_http::FusionRequestErrorClass::internalError;
            break;
    }

    return nx_http::FusionRequestResult(
        requestResultCode,
        QnLexical::serialized(resultCode),
        static_cast<int>(resultCode),
        QnLexical::serialized(resultCode));
}

ResultCode fusionRequestResultToResultCode(nx_http::FusionRequestResult result)
{
    if (result.errorClass == nx_http::FusionRequestErrorClass::noError)
        return ResultCode::ok;

    return static_cast<ResultCode>(result.errorDetail);
}


QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::notAuthorized, "notAuthorized")
    (ResultCode::forbidden, "forbidden")
    (ResultCode::accountNotActivated, "accountNotActivated")
    (ResultCode::accountBlocked, "accountBlocked")
    (ResultCode::notFound, "notFound")
    (ResultCode::alreadyExists, "alreadyExists")
    (ResultCode::dbError, "dbError")
    (ResultCode::networkError, "networkError")
    (ResultCode::notImplemented, "notImplemented")
    (ResultCode::unknownRealm, "unknownRealm")
    (ResultCode::badUsername, "badUsername")
    (ResultCode::badRequest, "badRequest")
    (ResultCode::invalidNonce, "invalidNonce")
    (ResultCode::serviceUnavailable, "serviceUnavailable")
    (ResultCode::credentialsRemovedPermanently, "credentialsRemovedPermanently")
    (ResultCode::invalidFormat, "invalidFormat")
    (ResultCode::unknownError, "unknownError")
);

std::string toString(ResultCode resultCode)
{
    return QnLexical::serialized(resultCode).toStdString();
}


}   //api
}   //cdb
}   //nx
