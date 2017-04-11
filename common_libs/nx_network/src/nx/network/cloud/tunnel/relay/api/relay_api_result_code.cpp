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
            NX_ASSERT(false);
            return hpm::api::NatTraversalResultCode::tcpConnectFailed;
    }
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::relay::api, ResultCode,
    (nx::cloud::relay::api::ResultCode::ok, "ok")
    (nx::cloud::relay::api::ResultCode::notFound, "notFound")
    (nx::cloud::relay::api::ResultCode::timedOut, "timedOut")
    (nx::cloud::relay::api::ResultCode::networkError, "networkError")
)
