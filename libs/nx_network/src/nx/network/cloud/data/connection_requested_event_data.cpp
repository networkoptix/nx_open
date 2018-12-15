#include "connection_requested_event_data.h"

namespace nx {
namespace hpm {
namespace api {

ConnectionRequestedEvent::ConnectionRequestedEvent():
    StunIndicationData(kMethod),
    connectionMethods(0),
    cloudConnectVersion(kCurrentCloudConnectVersion),
    isPersistent(false)
{
}

void ConnectionRequestedEvent::serializeAttributes(nx::network::stun::Message* const message)
{
    using namespace nx::network;

    message->newAttribute<stun::extension::attrs::ConnectionId>(std::move(connectSessionId));
    message->newAttribute<stun::extension::attrs::PeerId>(std::move(originatingPeerID));
    message->newAttribute<stun::extension::attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->newAttribute<stun::extension::attrs::ConnectionMethods>(nx::String::number(connectionMethods));
    params.serializeAttributes(message);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
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

} // namespace api
} // namespace hpm
} // namespace nx
