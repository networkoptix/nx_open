// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_result_data.h"

namespace nx::hpm::api {

using namespace network::stun::extension;

SystemError::ErrorCode toSystemErrorCode(NatTraversalResultCode resultCode)
{
    switch (resultCode)
    {
        case NatTraversalResultCode::ok:
            return SystemError::noError;

        case NatTraversalResultCode::endpointVerificationFailure:
            return SystemError::connectionRefused;

        case NatTraversalResultCode::udtConnectFailed:
        case NatTraversalResultCode::tcpConnectFailed:
        case NatTraversalResultCode::errorConnectingToRelay:
            return SystemError::connectionReset;

        case NatTraversalResultCode::notFoundOnRelay:
            return SystemError::hostNotFound;

        case NatTraversalResultCode::noResponseFromMediator:
        case NatTraversalResultCode::mediatorReportedError:
        case NatTraversalResultCode::noSynFromTargetPeer:
        case NatTraversalResultCode::targetPeerHasNoUdpAddress:
        case NatTraversalResultCode::noSuitableMethod:
            return SystemError::connectionAbort;

        default:
            return SystemError::connectionReset;
    }
}

//-------------------------------------------------------------------------------------------------

ConnectionResultRequest::ConnectionResultRequest():
    StunRequestData(kMethod)
{
}

void ConnectionResultRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::UdpHolePunchingResultCodeAttr>(
        static_cast<int>(resultCode));
    message->newAttribute<attrs::SystemErrorCodeAttr>(sysErrorCode);
    message->newAttribute<attrs::ConnectTypeAttr>(connectType);
}

bool ConnectionResultRequest::parseAttributes(const nx::network::stun::Message& message)
{
    // connectType is optional for backward compatibility.
    readEnumAttributeValue<attrs::ConnectTypeAttr>(message, &connectType);

    return readEnumAttributeValue<attrs::UdpHolePunchingResultCodeAttr>(message, &resultCode)
        && readEnumAttributeValue<attrs::SystemErrorCodeAttr>(message, &sysErrorCode)
        && readStringAttributeValue<attrs::ConnectionId>(message, &connectSessionId);
}

} // namespace nx::hpm::api
