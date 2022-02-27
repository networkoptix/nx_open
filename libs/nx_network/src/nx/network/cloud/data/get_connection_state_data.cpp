// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_connection_state_data.h"

namespace nx::hpm::api {

GetConnectionStateRequest::GetConnectionStateRequest():
    StunRequestData(kMethod)
{
}

void GetConnectionStateRequest::serializeAttributes(nx::network::stun::Message* const /*message*/)
{
}

bool GetConnectionStateRequest::parseAttributes(const nx::network::stun::Message& /*message*/)
{
    return true;
}

//-------------------------------------------------------------------------------------------------

GetConnectionStateResponse::GetConnectionStateResponse():
    StunResponseData(kMethod)
{
}

void GetConnectionStateResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->addAttribute(network::stun::extension::attrs::isListening, (int) state);
}

bool GetConnectionStateResponse::parseAttributes(const nx::network::stun::Message& message)
{
    return readAttributeValue(message, network::stun::extension::attrs::isListening, (int*) &state);
}

} // namespace nx::hpm::api
