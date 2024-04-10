// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <nx/network/cloud/cloud_connect_type.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/system_error.h>

#include "../tunnel/tunnel_connect_statistics.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

NX_REFLECTION_ENUM_CLASS(NatTraversalResultCode,
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
    relayProxyError
)

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

    std::string connectSessionId;
    NatTraversalResultCode resultCode = NatTraversalResultCode::ok;
    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    nx::network::cloud::TunnelConnectStatistics stats;
    std::chrono::milliseconds mediatorResponseTime;

    ConnectionResultRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
