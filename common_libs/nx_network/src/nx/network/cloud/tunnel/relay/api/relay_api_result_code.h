#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

enum class ResultCode
{
    ok,
    notFound,
    notAuthorized,
    timedOut,
    networkError,
    preemptiveConnectionCountAtMaximum,
    needRedirect,
    unknownError,
};

NX_NETWORK_API hpm::api::NatTraversalResultCode toNatTraversalResultCode(ResultCode);
NX_NETWORK_API nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode);
NX_NETWORK_API ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode);
NX_NETWORK_API SystemError::ErrorCode toSystemError(ResultCode resultCode);

NX_NETWORK_API nx::network::http::FusionRequestResult resultCodeToFusionRequestResult(
    api::ResultCode resultCode);
NX_NETWORK_API api::ResultCode fusionRequestResultToResultCode(
    nx::network::http::FusionRequestResult result);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx

/**
 * Not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec.
 */

NX_NETWORK_API void serialize(const nx::cloud::relay::api::ResultCode&, QString*);
NX_NETWORK_API bool deserialize(const QString&, nx::cloud::relay::api::ResultCode*);
