#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/http/httptypes.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

enum class ResultCode
{
    ok,
    notFound,
    timedOut,
    networkError,
    preemptiveConnectionCountAtMaximum,
};

NX_NETWORK_API hpm::api::NatTraversalResultCode toNatTraversalResultCode(ResultCode);
NX_NETWORK_API nx_http::StatusCode::Value toHttpStatusCode(ResultCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const nx::cloud::relay::api::ResultCode&, QString*);
