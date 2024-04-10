// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_result_code.h"

#include <nx/reflect/string_conversion.h>
#include "nx/network/cloud/data/connection_result_data.h"
#include "nx/network/http/http_status.h"

namespace nx::cloud::relay::api {

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
        case ResultCode::notAuthorized:
            return hpm::api::NatTraversalResultCode::errorConnectingToRelay;

        case ResultCode::proxyError:
            return hpm::api::NatTraversalResultCode::relayProxyError;

        default:
            return hpm::api::NatTraversalResultCode::tcpConnectFailed;
    }
}

nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx::network::http::StatusCode::ok;

        case ResultCode::notFound:
            return nx::network::http::StatusCode::notFound;

        case ResultCode::timedOut:
        case ResultCode::networkError:
            return nx::network::http::StatusCode::serviceUnavailable;

        case ResultCode::preemptiveConnectionCountAtMaximum:
            return nx::network::http::StatusCode::forbidden;

        default:
            return nx::network::http::StatusCode::badRequest;
    }
}

ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode)
{
    switch (statusCode)
    {
        case nx::network::http::StatusCode::ok:
            return ResultCode::ok;

        case nx::network::http::StatusCode::notFound:
            return ResultCode::notFound;

        case nx::network::http::StatusCode::serviceUnavailable:
            return ResultCode::networkError;

        case nx::network::http::StatusCode::badRequest:
            return ResultCode::networkError;

        case nx::network::http::StatusCode::forbidden:
        case nx::network::http::StatusCode::unauthorized:
            return ResultCode::notAuthorized;

        case nx::network::http::StatusCode::badGateway:
            return ResultCode::proxyError;

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

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << nx::reflect::toString(val);
}

//-------------------------------------------------------------------------------------------------
// Support of nx::network::http::ApiRequestResult

nx::network::http::ApiRequestResult resultCodeToFusionRequestResult(api::ResultCode resultCode)
{
    if (resultCode == ResultCode::ok || resultCode == ResultCode::needRedirect)
        return nx::network::http::ApiRequestResult();

    nx::network::http::ApiRequestErrorClass requestResultCode = nx::network::http::ApiRequestErrorClass::noError;
    switch (resultCode)
    {
        case ResultCode::notAuthorized:
            requestResultCode = nx::network::http::ApiRequestErrorClass::unauthorized;
            break;

        case ResultCode::notFound:
        case ResultCode::preemptiveConnectionCountAtMaximum:
            requestResultCode = nx::network::http::ApiRequestErrorClass::logicError;
            break;

        case ResultCode::timedOut:
        case ResultCode::networkError:
            requestResultCode = nx::network::http::ApiRequestErrorClass::ioError;
            break;

        default:
            requestResultCode = nx::network::http::ApiRequestErrorClass::internalError;
            break;
    }

    return nx::network::http::ApiRequestResult(
        requestResultCode,
        nx::reflect::toString(resultCode),
        static_cast<int>(resultCode),
        nx::reflect::toString(resultCode));
}

api::ResultCode fusionRequestResultToResultCode(nx::network::http::ApiRequestResult result)
{
    if (result.getErrorClass() == nx::network::http::ApiRequestErrorClass::noError)
        return ResultCode::ok;

    return static_cast<api::ResultCode>(result.getErrorDetail());
}

} // namespace nx::cloud::relay::api
