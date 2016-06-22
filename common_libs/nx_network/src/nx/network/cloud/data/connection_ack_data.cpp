/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#include "connection_ack_data.h"


namespace nx {
namespace hpm {
namespace api {

ConnectionAckRequest::ConnectionAckRequest()
:
    StunRequestData(kMethod),
    connectionMethods(0),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectionAckRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::cc::attrs::ConnectionMethods>(
        nx::String::number(connectionMethods));
    message->newAttribute< stun::cc::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
    message->addAttribute(stun::cc::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectionAckRequest::parseAttributes(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::cc::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readStringAttributeValue<stun::cc::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::cc::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::cc::attrs::UdtHpEndpointList>(message, &udpEndpointList);
}

}   //api
}   //hpm
}   //nx
