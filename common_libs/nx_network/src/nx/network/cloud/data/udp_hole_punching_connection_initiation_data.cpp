/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_data.h"


namespace nx {
namespace hpm {
namespace api {

void UdpHolePunchingSyn::serialize(nx::stun::Message* const message)
{
}

bool UdpHolePunchingSyn::parse(const nx::stun::Message& message)
{
    return true;
}

void UdpHolePunchingSynAck::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
}

bool UdpHolePunchingSynAck::parse(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::ConnectionId>(
        message,
        &connectSessionId);
}

}   //api
}   //hpm
}   //nx
