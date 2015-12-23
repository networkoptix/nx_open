/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_RESULT_CODE_H
#define NX_MEDIATOR_API_RESULT_CODE_H

#include <nx/network/stun/message.h>
#include <utils/common/model_functions_fwd.h>


namespace nx {
namespace network {
namespace cloud {
namespace api {

enum class ResultCode
{
    ok = 0,
    networkError,
    responseParseError,
    notFound,
    otherLogicError
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((FusionRequestErrorDetail), (lexical))

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const ResultCode&, QString*);

ResultCode NX_NETWORK_API fromStunErrorToResultCode(
    const nx::stun::attrs::ErrorDescription& errorDescription);

} // namespace api
} // namespace cloud
} // namespace network
} // namespace nx

#endif  //NX_MEDIATOR_API_RESULT_CODE_H
