#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {
namespace api {

// TODO: #ak too many different conversion functions here.

nx_http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
        case ResultCode::partialContent: //< Mapping to "200 OK" because request is not Partial Content from Http point of view.
            return nx_http::StatusCode::ok;

        case ResultCode::badUsername:
        case ResultCode::badRequest:
        case ResultCode::mergedSystemIsOffline:
        case ResultCode::invalidFormat:
            return nx_http::StatusCode::badRequest;

        case ResultCode::notAuthorized:
        case ResultCode::credentialsRemovedPermanently:
            return nx_http::StatusCode::unauthorized;

        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
        case ResultCode::notAllowedInCurrentState:
        case ResultCode::alreadyExists:
        case ResultCode::unknownRealm:
        case ResultCode::invalidNonce:
            return nx_http::StatusCode::forbidden;

        case ResultCode::notFound:
            return nx_http::StatusCode::notFound;

        case ResultCode::dbError:
        case ResultCode::networkError:
        case ResultCode::unknownError:
            return nx_http::StatusCode::internalServerError;

        case ResultCode::notImplemented:
            return nx_http::StatusCode::notImplemented;

        case ResultCode::serviceUnavailable:
        case ResultCode::retryLater:
            return nx_http::StatusCode::serviceUnavailable;
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
        case ResultCode::notAllowedInCurrentState:
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

std::string toString(ResultCode resultCode)
{
    return QnLexical::serialized(resultCode).toStdString();
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb::api, ResultCode,
    (nx::cdb::api::ResultCode::ok, "ok")
    (nx::cdb::api::ResultCode::notAuthorized, "notAuthorized")
    (nx::cdb::api::ResultCode::forbidden, "forbidden")
    (nx::cdb::api::ResultCode::accountNotActivated, "accountNotActivated")
    (nx::cdb::api::ResultCode::accountBlocked, "accountBlocked")
    (nx::cdb::api::ResultCode::notFound, "notFound")
    (nx::cdb::api::ResultCode::alreadyExists, "alreadyExists")
    (nx::cdb::api::ResultCode::dbError, "dbError")
    (nx::cdb::api::ResultCode::networkError, "networkError")
    (nx::cdb::api::ResultCode::notImplemented, "notImplemented")
    (nx::cdb::api::ResultCode::unknownRealm, "unknownRealm")
    (nx::cdb::api::ResultCode::badUsername, "badUsername")
    (nx::cdb::api::ResultCode::badRequest, "badRequest")
    (nx::cdb::api::ResultCode::invalidNonce, "invalidNonce")
    (nx::cdb::api::ResultCode::serviceUnavailable, "serviceUnavailable")
    (nx::cdb::api::ResultCode::notAllowedInCurrentState, "notAllowedInCurrentState")
    (nx::cdb::api::ResultCode::mergedSystemIsOffline, "mergedSystemIsOffline")
    (nx::cdb::api::ResultCode::credentialsRemovedPermanently, "credentialsRemovedPermanently")
    (nx::cdb::api::ResultCode::invalidFormat, "invalidFormat")
    (nx::cdb::api::ResultCode::retryLater, "retryLater")
    (nx::cdb::api::ResultCode::unknownError, "unknownError")
)

} // namespace api
} // namespace cdb
} // namespace nx
