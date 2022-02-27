// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_requested_event_data.h"

namespace nx::hpm::api {

ConnectionRequestedEvent::ConnectionRequestedEvent():
    StunIndicationData(kMethod)
{
}

void ConnectionRequestedEvent::serializeAttributes(nx::network::stun::Message* const message)
{
    using namespace nx::network;

    message->newAttribute<stun::extension::attrs::ConnectionId>(std::move(connectSessionId));
    message->newAttribute<stun::extension::attrs::PeerId>(std::move(originatingPeerID));
    message->newAttribute<stun::extension::attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->newAttribute<stun::extension::attrs::ConnectionMethods>(std::to_string(connectionMethods));
    params.serializeAttributes(message);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int) cloudConnectVersion);
    message->newAttribute<stun::extension::attrs::TcpReverseEndpointList>(std::move(tcpReverseEndpointList));
    message->addAttribute(stun::extension::attrs::isPersistent, isPersistent);
}

bool ConnectionRequestedEvent::parseAttributes(const nx::network::stun::Message& message)
{
    using namespace nx::network;

    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    const auto ret =
        readStringAttributeValue<stun::extension::attrs::ConnectionId>(message, &connectSessionId) &&
        readStringAttributeValue<stun::extension::attrs::PeerId>(message, &originatingPeerID) &&
        readAttributeValue<stun::extension::attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readIntAttributeValue<stun::extension::attrs::ConnectionMethods>(message, &connectionMethods) &&
        params.parseAttributes(message);

    // These do not appear in old versions:
    readAttributeValue<stun::extension::attrs::TcpReverseEndpointList>(message, &tcpReverseEndpointList);
    readAttributeValue(message, stun::extension::attrs::isPersistent, &isPersistent);

    return ret;
}

} // namespace nx::hpm::api
