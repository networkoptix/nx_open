/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "connect_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>


namespace nx {
namespace hpm {
namespace api {

ConnectRequest::ConnectRequest()
:
    StunRequestData(kMethod),
    connectionMethods(0),
    ignoreSourceAddress(false),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::HostName>(std::move(destinationHostName));
    message->newAttribute<stun::extension::attrs::PeerId>(std::move(originatingPeerId));
    message->newAttribute<stun::extension::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::extension::attrs::ConnectionMethods>(nx::String::number(connectionMethods));
    message->newAttribute<stun::extension::attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->addAttribute(stun::extension::attrs::ignoreSourceAddress, ignoreSourceAddress);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectRequest::parseAttributes(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version
    
    return
        readStringAttributeValue<stun::extension::attrs::HostName>(message, &destinationHostName) &&
        readStringAttributeValue<stun::extension::attrs::PeerId>(message, &originatingPeerId) &&
        readStringAttributeValue<stun::extension::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::extension::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::extension::attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readAttributeValue(message, stun::extension::attrs::ignoreSourceAddress, &ignoreSourceAddress);
}



ConnectResponse::ConnectResponse()
:
    StunResponseData(kMethod),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute< stun::extension::attrs::PublicEndpointList >(
        std::move(forwardedTcpEndpointList));
    message->newAttribute< stun::extension::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
    params.serializeAttributes(message);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectResponse::parseAttributes(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return 
        readAttributeValue<stun::extension::attrs::PublicEndpointList>(
            message, &forwardedTcpEndpointList) &&
        readAttributeValue<stun::extension::attrs::UdtHpEndpointList>(
            message, &udpEndpointList) &&
        params.parseAttributes(message);
}

}   //namespace api
}   //namespace hpm
}   //namespace nx
