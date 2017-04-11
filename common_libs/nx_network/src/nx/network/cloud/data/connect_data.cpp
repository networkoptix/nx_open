#include "connect_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx {
namespace hpm {
namespace api {

ConnectRequest::ConnectRequest():
    StunRequestData(kMethod),
    connectionMethods(0),
    ignoreSourceAddress(false),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectRequest::serializeAttributes(nx::stun::Message* const message)
{
    using namespace stun::extension;

    message->newAttribute<attrs::HostName>(std::move(destinationHostName));
    message->newAttribute<attrs::PeerId>(std::move(originatingPeerId));
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::ConnectionMethods>(nx::String::number(connectionMethods));
    message->newAttribute<attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->addAttribute(attrs::ignoreSourceAddress, ignoreSourceAddress);
    message->addAttribute(attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectRequest::parseAttributes(const nx::stun::Message& message)
{
    using namespace stun::extension;

    if (!readEnumAttributeValue(message, attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version
    
    return
        readStringAttributeValue<attrs::HostName>(message, &destinationHostName) &&
        readStringAttributeValue<attrs::PeerId>(message, &originatingPeerId) &&
        readStringAttributeValue<attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readAttributeValue(message, attrs::ignoreSourceAddress, &ignoreSourceAddress);
}

//-------------------------------------------------------------------------------------------------
// ConnectResponse

ConnectResponse::ConnectResponse():
    StunResponseData(kMethod),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ConnectResponse::serializeAttributes(nx::stun::Message* const message)
{
    using namespace stun::extension;

    message->newAttribute< attrs::PublicEndpointList >(std::move(forwardedTcpEndpointList));
    message->newAttribute< attrs::UdtHpEndpointList >(std::move(udpEndpointList));
    message->newAttribute< attrs::TrafficRelayEndpointList >(std::move(trafficRelayEndpointList));
    params.serializeAttributes(message);
    message->addAttribute(attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectResponse::parseAttributes(const nx::stun::Message& message)
{
    using namespace stun::extension;

    if (!readEnumAttributeValue(message, attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return 
        readAttributeValue<attrs::PublicEndpointList>(message, &forwardedTcpEndpointList) &&
        readAttributeValue<attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readAttributeValue<attrs::TrafficRelayEndpointList>(message, &trafficRelayEndpointList) &&
        params.parseAttributes(message);
}

} // namespace api
} // namespace hpm
} // namespace nx
