#include "relay_api_result_code.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

hpm::api::NatTraversalResultCode toNatTraversalResultCode(
    ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return hpm::api::NatTraversalResultCode::ok;

        case ResultCode::notFound:
            return hpm::api::NatTraversalResultCode::notFoundOnRelay;

        case ResultCode::timedOut:
        case ResultCode::networkError:
            return hpm::api::NatTraversalResultCode::errorConnectingToRelay;

        default:
            return hpm::api::NatTraversalResultCode::tcpConnectFailed;
    }
}

nx_http::StatusCode::Value toHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx_http::StatusCode::ok;

        case ResultCode::notFound:
            return nx_http::StatusCode::notFound;

        case ResultCode::timedOut:
        case ResultCode::networkError:
            return nx_http::StatusCode::serviceUnavailable;

        case ResultCode::preemptiveConnectionCountAtMaximum:
            return nx_http::StatusCode::forbidden;

        default:
            return nx_http::StatusCode::badRequest;
    }
}

ResultCode fromHttpStatusCode(nx_http::StatusCode::Value statusCode)
{
    switch (statusCode)
    {
        case nx_http::StatusCode::ok:
            return ResultCode::ok;

        case nx_http::StatusCode::notFound:
            return ResultCode::notFound;

        case nx_http::StatusCode::serviceUnavailable:
            return ResultCode::networkError;

        case nx_http::StatusCode::badRequest:
            return ResultCode::networkError;

        case nx_http::StatusCode::forbidden:
        case nx_http::StatusCode::unauthorized:
            return ResultCode::notAuthorized;

        default:
            return ResultCode::networkError;
    }
}

SystemError::ErrorCode toSystemError(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return SystemError::noError;

        case ResultCode::timedOut:
            return SystemError::timedOut;

        case ResultCode::notFound:
            return SystemError::hostNotFound;

        default:
            return SystemError::connectionReset;
    }
}

//-------------------------------------------------------------------------------------------------
// Support of nx_http::FusionRequestResult

nx_http::FusionRequestResult resultCodeToFusionRequestResult(api::ResultCode resultCode)
{
    if (resultCode == ResultCode::ok || resultCode == ResultCode::needRedirect)
        return nx_http::FusionRequestResult();

    nx_http::FusionRequestErrorClass requestResultCode = nx_http::FusionRequestErrorClass::noError;
    switch (resultCode)
    {
        case ResultCode::notAuthorized:
            requestResultCode = nx_http::FusionRequestErrorClass::unauthorized;
            break;

        case ResultCode::notFound:
        case ResultCode::preemptiveConnectionCountAtMaximum:
            requestResultCode = nx_http::FusionRequestErrorClass::logicError;
            break;

        case ResultCode::timedOut:
        case ResultCode::networkError:
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

api::ResultCode fusionRequestResultToResultCode(nx_http::FusionRequestResult result)
{
    if (result.errorClass == nx_http::FusionRequestErrorClass::noError)
        return ResultCode::ok;

    return static_cast<api::ResultCode>(result.errorDetail);
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx

using namespace nx::cloud::relay::api;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::relay::api, ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::notFound, "notFound")
    (ResultCode::notAuthorized, "notAuthorized")
    (ResultCode::timedOut, "timedOut")
    (ResultCode::networkError, "networkError")
    (ResultCode::preemptiveConnectionCountAtMaximum, "preemptiveConnectionCountAtMaximum")
    (ResultCode::unknownError, "unknownError")
)
