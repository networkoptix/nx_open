/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "connection_method.h"
#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ConnectionRequestedEvent
:
    public StunMessageData
{
public:
    nx::String connectSessionID;
    nx::String originatingPeerID;
    std::list<SocketAddress> udpEndpointList;
    ConnectionMethods connectionMethods;

    ConnectionRequestedEvent();

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
