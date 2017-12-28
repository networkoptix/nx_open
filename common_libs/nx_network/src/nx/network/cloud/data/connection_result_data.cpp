#include "connection_result_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace api {

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
    StunRequestData(kMethod),
    resultCode(NatTraversalResultCode::ok),
    sysErrorCode(SystemError::noError)
{
}

void ConnectionResultRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::UdpHolePunchingResultCodeAttr>(
        static_cast<int>(resultCode));
    message->newAttribute<attrs::SystemErrorCodeAttr>(sysErrorCode);
}

bool ConnectionResultRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return
        readEnumAttributeValue<attrs::UdpHolePunchingResultCodeAttr>(
            message, &resultCode) &&
        readEnumAttributeValue<attrs::SystemErrorCodeAttr>(
            message, &sysErrorCode) &&
        readStringAttributeValue<attrs::ConnectionId>(
            message,
            &connectSessionId);
}

} // namespace api
} // namespace hpm
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::hpm::api, NatTraversalResultCode,
    (nx::hpm::api::NatTraversalResultCode::ok, "ok")
    (nx::hpm::api::NatTraversalResultCode::noResponseFromMediator, "noResponseFromMediator")
    (nx::hpm::api::NatTraversalResultCode::mediatorReportedError, "mediatorReportedError")
    (nx::hpm::api::NatTraversalResultCode::targetPeerHasNoUdpAddress, "targetPeerHasNoUdpAddress")
    (nx::hpm::api::NatTraversalResultCode::noSynFromTargetPeer, "noSynFromTargetPeer")
    (nx::hpm::api::NatTraversalResultCode::udtConnectFailed, "udtConnectFailed")
    (nx::hpm::api::NatTraversalResultCode::udtConnectFailed, "tcpConnectFailed")
    (nx::hpm::api::NatTraversalResultCode::udtConnectFailed, "endpointVerificationFailure")
    (nx::hpm::api::NatTraversalResultCode::errorConnectingToRelay, "errorConnectingToRelay")
    (nx::hpm::api::NatTraversalResultCode::notFoundOnRelay, "notFoundOnRelay")
    (nx::hpm::api::NatTraversalResultCode::noSuitableMethod, "noSuitableMethod")
)
