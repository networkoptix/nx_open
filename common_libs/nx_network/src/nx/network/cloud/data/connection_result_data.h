/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
#define NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H

#include "stun_message_data.h"

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/systemerror.h>


namespace nx {
namespace hpm {
namespace api {

enum class NatTraversalResultCode
{
    ok = 0,
    noResponseFromMediator,
    mediatorReportedError,
    targetPeerHasNoUdpAddress,
    noSynFromTargetPeer,
    udtConnectFailed,
    tcpConnectFailed,
    endpointVerificationFailure
};

class NX_NETWORK_API ConnectionResultRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::connectionResult;

    nx::String connectSessionId;
    NatTraversalResultCode resultCode;
    SystemError::ErrorCode sysErrorCode;

    ConnectionResultRequest();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::hpm::api::NatTraversalResultCode)

}   //api
}   //hpm
}   //nx

void NX_NETWORK_API serialize(const nx::hpm::api::NatTraversalResultCode&, QString*);

#endif  //NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
