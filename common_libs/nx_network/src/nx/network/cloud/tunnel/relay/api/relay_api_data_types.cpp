#include "relay_api_data_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

namespace {

static constexpr const char* preemptiveConnectionCountHeaderName =
    "Nx-Relay-Preemptive-Connection-Count";

} // namespace

bool serializeToHeaders(nx_http::HttpHeaders* where, const BeginListeningResponse& what)
{
    where->emplace(
        preemptiveConnectionCountHeaderName,
        nx_http::StringType::number(what.preemptiveConnectionCount));
    return true;
}

bool serializeToHeaders(nx_http::HttpHeaders* where, const CreateClientSessionResponse& what)
{
    where->emplace("Location", what.actualRelayUrl.c_str());
    return true;
}

bool deserializeFromHeaders(const nx_http::HttpHeaders& from, BeginListeningResponse* what)
{
    auto it = from.find(preemptiveConnectionCountHeaderName);
    if (it == from.end())
        return false;
    what->preemptiveConnectionCount = it->second.toInt();

    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BeginListeningRequest)(BeginListeningResponse)(CreateClientSessionRequest) \
        (CreateClientSessionResponse)(ConnectToPeerRequest),
    (json),
    _Fields)

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
