// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resolve_peer_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx::hpm::api {

ResolvePeerRequest::ResolvePeerRequest(std::string _hostName):
    StunRequestData(kMethod),
    hostName(std::move(_hostName))
{
}

void ResolvePeerRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::HostName>(hostName);
}

bool ResolvePeerRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return readStringAttributeValue<network::stun::extension::attrs::HostName>(message, &hostName);
}

ResolvePeerResponse::ResolvePeerResponse():
    StunResponseData(kMethod),
    connectionMethods(0)
{
}

void ResolvePeerResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::PublicEndpointList>(std::move(endpoints));
    message->newAttribute<network::stun::extension::attrs::ConnectionMethods>(
        std::to_string(static_cast<qulonglong>(connectionMethods)));
}

bool ResolvePeerResponse::parseAttributes(const nx::network::stun::Message& message)
{
    if (!readAttributeValue<network::stun::extension::attrs::PublicEndpointList>(message, &endpoints))
        return false;
    std::string connectionMethodsStr;
    if (!readStringAttributeValue<network::stun::extension::attrs::ConnectionMethods>(
            message, &connectionMethodsStr))
    {
        return false;
    }
    connectionMethods = nx::utils::stoi(connectionMethodsStr);

    return true;
}

} // namespace nx::hpm::api
