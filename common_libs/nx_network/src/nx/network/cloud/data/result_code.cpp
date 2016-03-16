/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "result_code.h"

#include <nx/network/stun/cc/custom_stun.h>
#include <utils/common/model_functions.h>


namespace nx {
namespace hpm {
namespace api {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::networkError, "networkError")
    (ResultCode::responseParseError, "responseParseError")
    (ResultCode::notAuthorized, "notAuthorized")
    (ResultCode::badRequest, "badRequest")
    (ResultCode::notFound, "notFound")
    (ResultCode::otherLogicError, "otherLogicError")
    (ResultCode::notImplemented, "notImplemented")
    (ResultCode::noSuitableConnectionMethod, "noSuitableConnectionMethod")
    (ResultCode::timedOut, "timedOut")
    (ResultCode::serverConnectionBroken, "serverConnectionBroken")
    (ResultCode::noReplyFromServer, "noReplyFromServer")
    (ResultCode::badTransport, "badTransport")
    )

ResultCode fromStunErrorToResultCode(
    const nx::stun::attrs::ErrorDescription& errorDescription)
{
    switch (errorDescription.getCode())
    {
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
        case ResultCode::notFound:
            return nx::stun::cc::error::notFound;
        default:
            return nx::stun::error::serverError;
    }
}

} // namespace api
} // namespace hpm
} // namespace nx
