#include "udp_hole_punching_connection_initiation_data.h"

namespace nx {
namespace hpm {
namespace api {

constexpr const stun::extension::methods::Value UdpHolePunchingSynRequest::kMethod;

UdpHolePunchingSynRequest::UdpHolePunchingSynRequest():
    StunRequestData(kMethod)
{
}

void UdpHolePunchingSynRequest::serializeAttributes(nx::network::stun::Message* const /*message*/)
{
}

bool UdpHolePunchingSynRequest::parseAttributes(const nx::network::stun::Message& /*message*/)
{
    return true;
}


constexpr const stun::extension::methods::Value UdpHolePunchingSynResponse::kMethod;

UdpHolePunchingSynResponse::UdpHolePunchingSynResponse():
    StunResponseData(kMethod)
{
}

void UdpHolePunchingSynResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::ConnectionId>(connectSessionId);
}

bool UdpHolePunchingSynResponse::parseAttributes(const nx::network::stun::Message& message)
{
    return readStringAttributeValue<stun::extension::attrs::ConnectionId>(
        message,
        &connectSessionId);
}

} // namespace api
} // namespace hpm
} // namespace nx
