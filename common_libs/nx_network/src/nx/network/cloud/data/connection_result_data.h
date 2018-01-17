#pragma once

#include "stun_message_data.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/system_error.h>

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
    noSuitableMethod,
};

/**
 * This method is needed since socket API provides SystemError::ErrorCode as a result.
 */
NX_NETWORK_API SystemError::ErrorCode toSystemErrorCode(NatTraversalResultCode resultCode);

class NX_NETWORK_API ConnectionResultRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connectionResult;

    nx::String connectSessionId;
    NatTraversalResultCode resultCode;
    SystemError::ErrorCode sysErrorCode;

    ConnectionResultRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::hpm::api::NatTraversalResultCode)

} // namespace api
} // namespace hpm
} // namespace nx

NX_NETWORK_API void serialize(const nx::hpm::api::NatTraversalResultCode&, QString*);
