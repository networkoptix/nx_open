// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "udp_hole_punching_connection_initiation_data.h"

namespace nx::hpm::api {

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


constexpr const network::stun::extension::methods::Value UdpHolePunchingSynResponse::kMethod;

UdpHolePunchingSynResponse::UdpHolePunchingSynResponse():
    StunResponseData(kMethod)
{
}

void UdpHolePunchingSynResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::ConnectionId>(connectSessionId);
}

bool UdpHolePunchingSynResponse::parseAttributes(const nx::network::stun::Message& message)
{
    return readStringAttributeValue<network::stun::extension::attrs::ConnectionId>(
        message,
        &connectSessionId);
}

} // namespace nx::hpm::api
