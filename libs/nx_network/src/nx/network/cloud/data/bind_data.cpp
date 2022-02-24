// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bind_data.h"

namespace nx::hpm::api {

BindRequest::BindRequest(std::vector<network::SocketAddress> _publicEndpoints):
    StunRequestData(kMethod),
    publicEndpoints(std::move(_publicEndpoints))
{
}

void BindRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::PublicEndpointList>(
        std::move(publicEndpoints));
}

bool BindRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return readAttributeValue<network::stun::extension::attrs::PublicEndpointList>(
        message, &publicEndpoints);
}

//-------------------------------------------------------------------------------------------------

BindResponse::BindResponse():
    StunResponseData(kMethod)
{
}

void BindResponse::serializeAttributes(nx::network::stun::Message* const /*message*/)
{
}

bool BindResponse::parseAttributes(const nx::network::stun::Message& /*message*/)
{
    return true;
}

} // namespace nx::hpm::api
