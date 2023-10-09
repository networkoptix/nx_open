// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <nx/reflect/string_conversion.h>

namespace nx::cloud::db::api {

// TODO: #akolesnikov too many different conversion functions here.

nx::network::http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
        case ResultCode::partialContent: //< Mapping to "200 OK" because request is not Partial Content from Http point of view.
            return nx::network::http::StatusCode::ok;

        case ResultCode::noContent:
            return nx::network::http::StatusCode::noContent;

        case ResultCode::badRequest:
        case ResultCode::mergedSystemIsOffline:
        case ResultCode::invalidFormat:
        case ResultCode::userPasswordRequired:
            return nx::network::http::StatusCode::badRequest;

        case ResultCode::badUsername:
        case ResultCode::notAuthorized:
        case ResultCode::credentialsRemovedPermanently:
            return nx::network::http::StatusCode::unauthorized;

        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
        case ResultCode::secondFactorRequired:
        case ResultCode::notAllowedInCurrentState:
        case ResultCode::alreadyExists:
        case ResultCode::unknownRealm:
        case ResultCode::invalidNonce:
        case ResultCode::invalidTotp:
        case ResultCode::invalidBackupCode:
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

        case ResultCode::updateConflict:
            return nx::network::http::StatusCode::conflict;
    }

    return nx::network::http::StatusCode::internalServerError;
}

ResultCode httpStatusCodeToResultCode(nx::network::http::StatusCode::Value statusCode)
{
    switch (statusCode)
    {
        case nx::network::http::StatusCode::ok:
            return ResultCode::ok;
        case nx::network::http::StatusCode::noContent:
            return ResultCode::noContent;
        case nx::network::http::StatusCode::unauthorized:
            return ResultCode::notAuthorized;
        case nx::network::http::StatusCode::proxyAuthenticationRequired:
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
        case nx::network::http::StatusCode::conflict:
            return ResultCode::updateConflict;
        default:
            return ResultCode::unknownError;
    }
}

nx::network::http::ApiRequestResult apiResultToFusionRequestResult(Result result)
{
    if (result == ResultCode::ok)
        return nx::network::http::ApiRequestResult();

    if (result == ResultCode::noContent)
    {
        nx::network::http::ApiRequestResult res(
            nx::network::http::ApiRequestErrorClass::logicError,
            toString(result.code),
            static_cast<int>(result.code),
            result.description.empty()
                ? toString(result.code)
                : result.description);
        res.setHttpStatusCode(nx::network::http::StatusCode::noContent);
        return res;
    }

    nx::network::http::ApiRequestErrorClass requestResultCode =
        nx::network::http::ApiRequestErrorClass::noError;
    switch (result.code)
    {
        case ResultCode::notAuthorized:
        case ResultCode::forbidden:
        case ResultCode::accountNotActivated:
        case ResultCode::accountBlocked:
        case ResultCode::secondFactorRequired:
        case ResultCode::badUsername:
        case ResultCode::invalidNonce:
        case ResultCode::unknownRealm:
        case ResultCode::credentialsRemovedPermanently:
        case ResultCode::notAllowedInCurrentState:
            requestResultCode = nx::network::http::ApiRequestErrorClass::unauthorized;
            break;

        case ResultCode::badRequest:
        case ResultCode::userPasswordRequired:
            requestResultCode = nx::network::http::ApiRequestErrorClass::badRequest;
            break;

        case ResultCode::notFound:
        case ResultCode::alreadyExists:
            requestResultCode = nx::network::http::ApiRequestErrorClass::logicError;
            break;

        case ResultCode::dbError:
        case ResultCode::networkError:
        case ResultCode::serviceUnavailable:
            requestResultCode = nx::network::http::ApiRequestErrorClass::ioError;
            break;

        default:
            requestResultCode = nx::network::http::ApiRequestErrorClass::internalError;
            break;
    }

    return nx::network::http::ApiRequestResult(
        requestResultCode,
        toString(result.code),
        static_cast<int>(result.code),
        result.description.empty()
            ? toString(result.code)
            : result.description);
}

ResultCode fusionRequestResultToResultCode(nx::network::http::ApiRequestResult result)
{
    if (result.getErrorClass() == nx::network::http::ApiRequestErrorClass::noError)
        return ResultCode::ok;

    return static_cast<ResultCode>(result.getErrorDetail());
}

std::string toString(ResultCode resultCode)
{
    return nx::reflect::toString(resultCode);
}

} // namespace nx::cloud::db::api
