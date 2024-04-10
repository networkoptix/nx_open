// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_result_data.h"

#include <chrono>

#include <nx/network/stun/extension/stun_extension_types.h>

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
    message->newAttribute<attrs::ConnectTypeAttr>(stats.connectType);
    message->newAttribute<attrs::TunnelConnectResponseTime>(stats.responseTime);
    message->newAttribute<attrs::HostName>(stats.remoteAddress);
    if (stats.httpStatusCode)
        message->newAttribute<attrs::HttpStatusCode>(*stats.httpStatusCode);

    message->newAttribute<attrs::MediatorResponseTime>(mediatorResponseTime);
}

bool ConnectionResultRequest::parseAttributes(const nx::network::stun::Message& message)
{
    // connectType is optional for backward compatibility.
    readEnumAttributeValue<attrs::ConnectTypeAttr>(message, &stats.connectType);

    readDurationAttributeValue(
        message,
        nx::network::stun::extension::attrs::tunnelConnectResponseTime,
        &stats.responseTime);

    readStringAttributeValue<attrs::HostName>(message, &stats.remoteAddress);

    readDurationAttributeValue(
        message,
        nx::network::stun::extension::attrs::mediatorResponseTime,
        &mediatorResponseTime);

    int statsHttpStatusCode;
    if (readAttributeValue<attrs::HttpStatusCode>(message, &statsHttpStatusCode))
        stats.httpStatusCode = nx::network::http::StatusCode::Value(statsHttpStatusCode);

    return readEnumAttributeValue<attrs::UdpHolePunchingResultCodeAttr>(message, &resultCode)
        && readEnumAttributeValue<attrs::SystemErrorCodeAttr>(message, &sysErrorCode)
        && readStringAttributeValue<attrs::ConnectionId>(message, &connectSessionId);
}

} // namespace nx::hpm::api
