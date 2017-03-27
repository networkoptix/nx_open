#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/cloud/data/connection_result_data.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace api {

enum class ResultCode
{
    ok,
    notFound,
    timedOut,
    networkError,
};

NX_NETWORK_API hpm::api::NatTraversalResultCode toNatTraversalResultCode(ResultCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const nx::network::cloud::relay::api::ResultCode&, QString*);
