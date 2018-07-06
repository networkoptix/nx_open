#pragma once

#include <nx/network/stun/message.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace hpm {
namespace api {

enum class ResultCode
{
    ok = 0,
    networkError,
    responseParseError,
    notAuthorized,
    badRequest,
    notFound,
    otherLogicError,
    notImplemented,
    noSuitableConnectionMethod,
    timedOut,
    serverConnectionBroken,
    noReplyFromServer,
    badTransport,
    interrupted,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

ResultCode NX_NETWORK_API fromStunErrorToResultCode(
    const nx::network::stun::attrs::ErrorCode& errorCode);
int NX_NETWORK_API resultCodeToStunErrorCode(ResultCode resultCode);

QString NX_NETWORK_API toString(ResultCode code);

} // namespace api
} // namespace hpm
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::ResultCode, (lexical), NX_NETWORK_API)
