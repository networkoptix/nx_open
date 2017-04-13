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

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx

using namespace nx::cloud::relay::api;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::relay::api, ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::notFound, "notFound")
    (ResultCode::timedOut, "timedOut")
    (ResultCode::networkError, "networkError")
    (ResultCode::preemptiveConnectionCountAtMaximum, "preemptiveConnectionCountAtMaximum")
)
