// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/http/http_status.h>
#include <nx/network/http/server/api_request_result.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/system_error.h>

namespace nx::cloud::relay::api {

NX_REFLECTION_ENUM_CLASS(ResultCode,
    ok,
    notFound,
    notAuthorized,
    timedOut,
    networkError,
    preemptiveConnectionCountAtMaximum,
    needRedirect,
    unknownError,
    proxyError
)

NX_NETWORK_API hpm::api::NatTraversalResultCode toNatTraversalResultCode(ResultCode);
NX_NETWORK_API nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode);
NX_NETWORK_API ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode);
NX_NETWORK_API SystemError::ErrorCode toSystemError(ResultCode resultCode);
NX_NETWORK_API void PrintTo(ResultCode val, ::std::ostream* os);

NX_NETWORK_API nx::network::http::ApiRequestResult resultCodeToFusionRequestResult(
    api::ResultCode resultCode);
NX_NETWORK_API api::ResultCode fusionRequestResultToResultCode(
    nx::network::http::ApiRequestResult result);

} // namespace nx::cloud::relay::api
