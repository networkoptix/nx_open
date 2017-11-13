#include "relay_api_data_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

namespace {

static constexpr const char* kPreemptiveConnectionCountHeaderName =
    "Nx-Relay-Preemptive-Connection-Count";
static constexpr int kPreemptiveConnectionCountDefault = 7;

static constexpr const char* kTcpKeepAliveHeaderName =
    "Nx-Relay-Tcp-Connection-Keep-Alive";

} // namespace

BeginListeningResponse::BeginListeningResponse():
    preemptiveConnectionCount(kPreemptiveConnectionCountDefault)
{
}

bool BeginListeningResponse::operator==(const BeginListeningResponse& right) const
{
    return preemptiveConnectionCount == right.preemptiveConnectionCount
        && keepAliveOptions == right.keepAliveOptions;
}

bool serializeToHeaders(nx_http::HttpHeaders* where, const BeginListeningResponse& what)
{
    where->emplace(
        kPreemptiveConnectionCountHeaderName,
        nx_http::StringType::number(what.preemptiveConnectionCount));

    if (what.keepAliveOptions)
        where->emplace(kTcpKeepAliveHeaderName, what.keepAliveOptions->toString().toUtf8());

    return true;
}

bool deserializeFromHeaders(const nx_http::HttpHeaders& from, BeginListeningResponse* what)
{
    auto it = from.find(kPreemptiveConnectionCountHeaderName);
    if (it != from.end())
        what->preemptiveConnectionCount = it->second.toInt();

    it = from.find(kTcpKeepAliveHeaderName);
    if (it != from.end())
        what->keepAliveOptions = KeepAliveOptions::fromString(QString::fromUtf8(it->second));

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
