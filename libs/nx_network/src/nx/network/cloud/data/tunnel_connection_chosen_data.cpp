// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tunnel_connection_chosen_data.h"

namespace nx::hpm::api {

TunnelConnectionChosenRequest::TunnelConnectionChosenRequest():
    StunRequestData(kMethod)
{
}

void TunnelConnectionChosenRequest::serializeAttributes(nx::network::stun::Message* const /*message*/)
{
}

bool TunnelConnectionChosenRequest::parseAttributes(const nx::network::stun::Message& /*message*/)
{
    return true;
}

TunnelConnectionChosenResponse::TunnelConnectionChosenResponse():
    StunResponseData(kMethod)
{
}

void TunnelConnectionChosenResponse::serializeAttributes(nx::network::stun::Message* const /*message*/)
{
}

bool TunnelConnectionChosenResponse::parseAttributes(const nx::network::stun::Message& /*message*/)
{
    return true;
}

} // namespace nx::hpm::api
