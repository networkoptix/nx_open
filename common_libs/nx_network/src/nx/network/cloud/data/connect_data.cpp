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
{
}

void ConnectRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::HostName>(hostName);
    message->newAttribute<stun::cc::attrs::PeerId>("SomeClientId"); //TODO #ak
}

bool ConnectRequest::parse(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::HostName>(message, &hostName)
        && readStringAttributeValue<stun::cc::attrs::PeerId>(message, &peerID);
}



ConnectResponse::ConnectResponse()
{
}

void ConnectResponse::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(std::move(endpoints));
}

bool ConnectResponse::parse(const nx::stun::Message& message)
{
    return readAttributeValue<stun::cc::attrs::PublicEndpointList>(message, &endpoints);
}

}   //api
}   //hpm
}   //nx
