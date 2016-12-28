#include "result_code.h"

#include <nx/network/stun/cc/custom_stun.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace api {

ResultCode fromStunErrorToResultCode(
    const nx::stun::attrs::ErrorCode& errorCode)
{
    switch (errorCode.getCode())
    {
        case nx::stun::error::badRequest:
            return ResultCode::badRequest;
        case nx::stun::error::unauthtorized:
            return ResultCode::notAuthorized;
        case nx::stun::cc::error::notFound:
            return ResultCode::notFound;
        default:
            return ResultCode::otherLogicError;
    }
}

int resultCodeToStunErrorCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::badRequest:
            return nx::stun::error::badRequest;
        case ResultCode::notAuthorized:
            return nx::stun::error::unauthtorized;
        case ResultCode::notFound:
            return nx::stun::cc::error::notFound;
        default:
            return nx::stun::error::serverError;
    }
}

QString toString(ResultCode code)
{
    QString s;
    serialize(code, &s);
    return s;
}

} // namespace api
} // namespace hpm
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::hpm::api, ResultCode,
    (nx::hpm::api::ResultCode::ok, "ok")
    (nx::hpm::api::ResultCode::networkError, "networkError")
    (nx::hpm::api::ResultCode::responseParseError, "responseParseError")
    (nx::hpm::api::ResultCode::notAuthorized, "notAuthorized")
    (nx::hpm::api::ResultCode::badRequest, "badRequest")
    (nx::hpm::api::ResultCode::notFound, "notFound")
    (nx::hpm::api::ResultCode::otherLogicError, "otherLogicError")
    (nx::hpm::api::ResultCode::notImplemented, "notImplemented")
    (nx::hpm::api::ResultCode::noSuitableConnectionMethod, "noSuitableConnectionMethod")
    (nx::hpm::api::ResultCode::timedOut, "timedOut")
    (nx::hpm::api::ResultCode::serverConnectionBroken, "serverConnectionBroken")
    (nx::hpm::api::ResultCode::noReplyFromServer, "noReplyFromServer")
    (nx::hpm::api::ResultCode::badTransport, "badTransport")
)
