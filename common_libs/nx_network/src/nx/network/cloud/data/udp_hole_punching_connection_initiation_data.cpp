/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_data.h"


namespace nx {
namespace hpm {
namespace api {

constexpr const stun::cc::methods::Value UdpHolePunchingSynRequest::kMethod;

UdpHolePunchingSynRequest::UdpHolePunchingSynRequest()
:
    StunRequestData(kMethod)
{
}

void UdpHolePunchingSynRequest::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool UdpHolePunchingSynRequest::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}


constexpr const stun::cc::methods::Value UdpHolePunchingSynResponse::kMethod;

UdpHolePunchingSynResponse::UdpHolePunchingSynResponse()
:
    StunResponseData(kMethod)
{
}

void UdpHolePunchingSynResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
}

bool UdpHolePunchingSynResponse::parseAttributes(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::ConnectionId>(
        message,
        &connectSessionId);
}

}   //api
}   //hpm
}   //nx
