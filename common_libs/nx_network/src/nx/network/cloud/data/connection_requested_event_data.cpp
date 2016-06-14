/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#include "connection_requested_event_data.h"


namespace nx {
namespace hpm {
namespace api {

ConnectionRequestedEvent::ConnectionRequestedEvent()
:
    connectionMethods(0),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectionRequestedEvent::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionId>(std::move(connectSessionId));
    message->newAttribute<stun::cc::attrs::PeerId>(std::move(originatingPeerID));
    message->newAttribute<stun::cc::attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->newAttribute<stun::cc::attrs::ConnectionMethods>(nx::String::number(connectionMethods));
    params.serialize(message);
    message->addAttribute(stun::cc::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectionRequestedEvent::parse(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::cc::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readStringAttributeValue<stun::cc::attrs::ConnectionId>(message, &connectSessionId) &&
        readStringAttributeValue<stun::cc::attrs::PeerId>(message, &originatingPeerID) &&
        readAttributeValue<stun::cc::attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readIntAttributeValue<stun::cc::attrs::ConnectionMethods>(message, &connectionMethods) &&
        params.parse(message);
}

}   //api
}   //hpm
}   //nx
