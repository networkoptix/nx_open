#include "connection_ack_data.h"

namespace nx {
namespace hpm {
namespace api {

ConnectionAckRequest::ConnectionAckRequest():
    StunRequestData(kMethod),
    connectionMethods(0),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectionAckRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::extension::attrs::ConnectionMethods>(
        nx::String::number(connectionMethods));
    message->newAttribute< stun::extension::attrs::PublicEndpointList >(
        std::move(forwardedTcpEndpointList));
    message->newAttribute< stun::extension::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectionAckRequest::parseAttributes(const nx::network::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readAttributeValue<stun::extension::attrs::PublicEndpointList>(
            message, &forwardedTcpEndpointList) &&
        readStringAttributeValue<stun::extension::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::extension::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::extension::attrs::UdtHpEndpointList>(message, &udpEndpointList);
}

} // namespace api
} // namespace hpm
} // namespace nx
