/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "connect_data.h"

#include <nx/network/stun/cc/custom_stun.h>


namespace nx {
namespace hpm {
namespace api {

ConnectRequest::ConnectRequest()
:
    connectionMethods(0),
    ignoreSourceAddress(false)
{
}

void ConnectRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::HostName>(std::move(destinationHostName));
    message->newAttribute<stun::cc::attrs::PeerId>(std::move(originatingPeerID));
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::cc::attrs::ConnectionMethods>(nx::String::number(connectionMethods));
    message->newAttribute<stun::cc::attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->addAttribute(stun::cc::attrs::ignoreSourceAddress, ignoreSourceAddress);
}

bool ConnectRequest::parse(const nx::stun::Message& message)
{
    return
        readStringAttributeValue<stun::cc::attrs::HostName>(message, &destinationHostName) &&
        readStringAttributeValue<stun::cc::attrs::PeerId>(message, &originatingPeerID) &&
        readStringAttributeValue<stun::cc::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::cc::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::cc::attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readAttributeValue(message, stun::cc::attrs::ignoreSourceAddress, &ignoreSourceAddress);
}



ConnectResponse::ConnectResponse()
{
}

void ConnectResponse::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(
        std::move(publicTcpEndpointList));
    message->newAttribute< stun::cc::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
    params.serialize(message);
}

bool ConnectResponse::parse(const nx::stun::Message& message)
{
    return 
        readAttributeValue<stun::cc::attrs::PublicEndpointList>(
            message, &publicTcpEndpointList) &&
        readAttributeValue<stun::cc::attrs::UdtHpEndpointList>(
            message, &udpEndpointList) &&
        params.parse(message);
}

}   //namespace api
}   //namespace hpm
}   //namespace nx
