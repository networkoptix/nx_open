/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_data.h"


namespace nx {
namespace hpm {
namespace api {

const stun::cc::methods::Value UdpHolePunchingSynRequest::kMethod;

void UdpHolePunchingSynRequest::serialize(nx::stun::Message* const message)
{
    message->header.messageClass = stun::MessageClass::request;
    message->header.method = kMethod;
}

bool UdpHolePunchingSynRequest::parse(const nx::stun::Message& /*message*/)
{
    return true;
}


const stun::cc::methods::Value UdpHolePunchingSynResponse::kMethod;

void UdpHolePunchingSynResponse::serialize(nx::stun::Message* const message)
{
    message->header.messageClass = stun::MessageClass::successResponse;
    message->header.method = kMethod;
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
}

bool UdpHolePunchingSynResponse::parse(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::ConnectionId>(
        message,
        &connectSessionId);
}

}   //api
}   //hpm
}   //nx
