/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "connection_result_data.h"

#include <nx/fusion/model_functions.h>


namespace nx {
namespace hpm {
namespace api {

using namespace stun::cc;


QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(NatTraversalResultCode,
    (NatTraversalResultCode::ok, "ok")
    (NatTraversalResultCode::noResponseFromMediator, "noResponseFromMediator")
    (NatTraversalResultCode::mediatorReportedError, "mediatorReportedError")
    (NatTraversalResultCode::targetPeerHasNoUdpAddress, "targetPeerHasNoUdpAddress")
    (NatTraversalResultCode::noSynFromTargetPeer, "noSynFromTargetPeer")
    (NatTraversalResultCode::udtConnectFailed, "udtConnectFailed")
    )


ConnectionResultRequest::ConnectionResultRequest()
:
    StunRequestData(kMethod),
    resultCode(NatTraversalResultCode::ok),
    sysErrorCode(SystemError::noError)
{
}

void ConnectionResultRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::UdpHolePunchingResultCodeAttr>(
        static_cast<int>(resultCode));
    message->newAttribute<attrs::SystemErrorCodeAttr>(sysErrorCode);
}

bool ConnectionResultRequest::parseAttributes(const nx::stun::Message& message)
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

}   //api
}   //hpm
}   //nx
