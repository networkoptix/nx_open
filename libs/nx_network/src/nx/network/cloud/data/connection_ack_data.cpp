// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_ack_data.h"

namespace nx::hpm::api {

ConnectionAckRequest::ConnectionAckRequest():
    StunRequestData(kMethod)
{
}

void ConnectionAckRequest::serializeAttributes(network::stun::Message* const message)
{
    using namespace nx::network;

    message->newAttribute<stun::extension::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::extension::attrs::ConnectionMethods>(
        std::to_string(connectionMethods));
    message->newAttribute< stun::extension::attrs::PublicEndpointList >(
        std::move(forwardedTcpEndpointList));
    message->newAttribute< stun::extension::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectionAckRequest::parseAttributes(const nx::network::stun::Message& message)
{
    using namespace nx::network;

    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readAttributeValue<stun::extension::attrs::PublicEndpointList>(
            message, &forwardedTcpEndpointList) &&
        readStringAttributeValue<stun::extension::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::extension::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::extension::attrs::UdtHpEndpointList>(message, &udpEndpointList);
}

} // namespace nx::hpm::api
