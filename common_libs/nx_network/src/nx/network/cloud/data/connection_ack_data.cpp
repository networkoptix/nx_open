/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#include "connection_ack_data.h"

#include "connection_ack_data.h"


namespace nx {
namespace hpm {
namespace api {

ConnectionAckRequest::ConnectionAckRequest()
:
    connectionMethods(0)
{
}

void ConnectionAckRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionId>(connectSessionId);
    message->newAttribute<stun::cc::attrs::ConnectionMethods>(
        nx::String::number(connectionMethods));
    message->newAttribute< stun::cc::attrs::UdtHpEndpointList >(
        std::move(udpEndpointList));
}

bool ConnectionAckRequest::parse(const nx::stun::Message& message)
{
    return
        readStringAttributeValue<stun::cc::attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<stun::cc::attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<stun::cc::attrs::UdtHpEndpointList>(message, &udpEndpointList);
}

}   //api
}   //hpm
}   //nx
