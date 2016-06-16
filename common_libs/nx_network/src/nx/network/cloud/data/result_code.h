/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_RESULT_CODE_H
#define NX_MEDIATOR_API_RESULT_CODE_H

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
    badTransport
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((FusionRequestErrorDetail), (lexical))

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const ResultCode&, QString*);

ResultCode NX_NETWORK_API fromStunErrorToResultCode(
    const nx::stun::attrs::ErrorDescription& errorDescription);
int NX_NETWORK_API resultCodeToStunErrorCode(ResultCode resultCode);

} // namespace api
} // namespace hpm
} // namespace nx

#endif  //NX_MEDIATOR_API_RESULT_CODE_H
