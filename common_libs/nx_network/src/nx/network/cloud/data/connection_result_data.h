#pragma once

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
    endpointVerificationFailure,
    errorConnectingToRelay,
    notFoundOnRelay,
};

class NX_NETWORK_API ConnectionResultRequest:
    public StunRequestData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::connectionResult;

    nx::String connectSessionId;
    NatTraversalResultCode resultCode;
    SystemError::ErrorCode sysErrorCode;

    ConnectionResultRequest();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::hpm::api::NatTraversalResultCode)

} // namespace api
} // namespace hpm
} // namespace nx

void NX_NETWORK_API serialize(const nx::hpm::api::NatTraversalResultCode&, QString*);
