/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
#define NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H

#include "stun_message_data.h"

#include <utils/common/model_functions_fwd.h>
#include <utils/common/systemerror.h>


namespace nx {
namespace hpm {
namespace api {

enum class UdpHolePunchingResultCode
{
    ok,
    noResponseFromMediator,
    mediatorReportedError,
    targetPeerHasNoUdpAddress,
    noSynFromTargetPeer,
    udtConnectFailed
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(UdpHolePunchingResultCode)
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((FusionRequestErrorDetail), (lexical))
//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const UdpHolePunchingResultCode&, QString*);

class NX_NETWORK_API ConnectionResultRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::connectionResult;

    nx::String connectSessionId;
    UdpHolePunchingResultCode resultCode;
    SystemError::ErrorCode sysErrorCode;

    ConnectionResultRequest();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //api
}   //hpm
}   //nx

#endif  //NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
