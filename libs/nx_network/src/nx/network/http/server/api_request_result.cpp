// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api_request_result.h"

namespace nx::network::http {

ApiRequestErrorClass fromHttpStatus(StatusCode::Value statusCode)
{
    if (StatusCode::isSuccessCode(statusCode))
        return ApiRequestErrorClass::noError;

    switch (statusCode)
    {
        case StatusCode::unauthorized:
            return ApiRequestErrorClass::unauthorized;
        case StatusCode::notFound:
            return ApiRequestErrorClass::notFound;
        default:
            if (statusCode % 100 == 4)
                return ApiRequestErrorClass::badRequest;
            return ApiRequestErrorClass::internalError;
    }
}

} // namespace nx::network::http
