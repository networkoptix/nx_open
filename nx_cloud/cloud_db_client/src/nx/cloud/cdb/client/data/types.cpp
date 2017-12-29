#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {
namespace api {

// TODO: #ak too many different conversion functions here.

nx::network::http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
        case ResultCode::partialContent: //< Mapping to "200 OK" because request is not Partial Content from Http point of view.
            return nx::network::http::StatusCode::ok;

        case ResultCode::badUsername:
        case ResultCode::badRequest:
        case ResultCode::mergedSystemIsOffline:
        case ResultCode::invalidFormat:
            return nx::network::http::StatusCode::badRequest;

        case ResultCode::notAuthorized:
        case ResultCode::credentialsRemovedPermanently:
            return nx::network::http::StatusCode::unauthorized;

        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
        case ResultCode::notAllowedInCurrentState:
        case ResultCode::alreadyExists:
        case ResultCode::unknownRealm:
        case ResultCode::invalidNonce:
            return nx::network::http::StatusCode::forbidden;

        case ResultCode::notFound:
            return nx::network::http::StatusCode::notFound;

        case ResultCode::dbError:
        case ResultCode::networkError:
        case ResultCode::unknownError:
            return nx::network::http::StatusCode::internalServerError;

        case ResultCode::notImplemented:
            return nx::network::http::StatusCode::notImplemented;

        case ResultCode::serviceUnavailable:
        case ResultCode::retryLater:
        case ResultCode::vmsRequestFailure:
            return nx::network::http::StatusCode::serviceUnavailable;
    }

    return nx::network::http::StatusCode::internalServerError;
}

ResultCode httpStatusCodeToResultCode(nx::network::http::StatusCode::Value statusCode)
{
    switch (statusCode)
    {
        case nx::network::http::StatusCode::ok:
            return ResultCode::ok;
        case nx::network::http::StatusCode::unauthorized:
            return ResultCode::notAuthorized;
        case nx::network::http::StatusCode::forbidden:
            return ResultCode::forbidden;
        case nx::network::http::StatusCode::notFound:
            return ResultCode::notFound;
        case nx::network::http::StatusCode::internalServerError:
            return ResultCode::dbError;
        case nx::network::http::StatusCode::notImplemented:
            return ResultCode::notImplemented;
        case nx::network::http::StatusCode::serviceUnavailable:
            return ResultCode::serviceUnavailable;
        default:
            return ResultCode::unknownError;
    }
}

nx::network::http::FusionRequestResult resultCodeToFusionRequestResult(ResultCode resultCode)
{
    if (resultCode == ResultCode::ok)
        return nx::network::http::FusionRequestResult();

    nx::network::http::FusionRequestErrorClass requestResultCode = nx::network::http::FusionRequestErrorClass::noError;
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
            requestResultCode = nx::network::http::FusionRequestErrorClass::unauthorized;
            break;

        case ResultCode::notFound:
        case ResultCode::alreadyExists:
            requestResultCode = nx::network::http::FusionRequestErrorClass::logicError;
            break;

        case ResultCode::dbError:
        case ResultCode::networkError:
        case ResultCode::serviceUnavailable:
            requestResultCode = nx::network::http::FusionRequestErrorClass::ioError;
            break;

        default:
            requestResultCode = nx::network::http::FusionRequestErrorClass::internalError;
            break;
    }

    return nx::network::http::FusionRequestResult(
        requestResultCode,
        QnLexical::serialized(resultCode),
        static_cast<int>(resultCode),
        QnLexical::serialized(resultCode));
}

ResultCode fusionRequestResultToResultCode(nx::network::http::FusionRequestResult result)
{
    if (result.errorClass == nx::network::http::FusionRequestErrorClass::noError)
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
    (nx::cdb::api::ResultCode::vmsRequestFailure, "vmsRequestFailure")
    (nx::cdb::api::ResultCode::credentialsRemovedPermanently, "credentialsRemovedPermanently")
    (nx::cdb::api::ResultCode::invalidFormat, "invalidFormat")
    (nx::cdb::api::ResultCode::retryLater, "retryLater")
    (nx::cdb::api::ResultCode::unknownError, "unknownError")
)

} // namespace api
} // namespace cdb
} // namespace nx
