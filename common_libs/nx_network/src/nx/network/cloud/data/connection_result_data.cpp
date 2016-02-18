/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "connection_result_data.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace hpm {
namespace api {

using namespace stun::cc;


QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(UdpHolePunchingResultCode,
    (UdpHolePunchingResultCode::ok, "ok")
    (UdpHolePunchingResultCode::noResponseFromMediator, "noResponseFromMediator")
    (UdpHolePunchingResultCode::mediatorReportedError, "mediatorReportedError")
    (UdpHolePunchingResultCode::targetPeerHasNoUdpAddress, "targetPeerHasNoUdpAddress")
    (UdpHolePunchingResultCode::noSynFromTargetPeer, "noSynFromTargetPeer")
    (UdpHolePunchingResultCode::udtConnectFailed, "udtConnectFailed")
    )


ConnectionResultRequest::ConnectionResultRequest()
:
    resultCode(UdpHolePunchingResultCode::ok),
    sysErrorCode(SystemError::noError)
{
}

void ConnectionResultRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::UdpHolePunchingResultCodeAttr>(
        static_cast<int>(resultCode));
    message->newAttribute<attrs::SystemErrorCodeAttr>(sysErrorCode);
}

bool ConnectionResultRequest::parse(const nx::stun::Message& message)
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
