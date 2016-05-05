/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ConnectionRequestedEvent
:
    public StunMessageData
{
public:
    nx::String connectSessionId;
    nx::String originatingPeerID;
    std::list<SocketAddress> udpEndpointList;   ///< Peer UDP addresses
    ConnectionMethods connectionMethods;        ///< All requestd connection types
    ConnectionParameters params;

    ConnectionRequestedEvent();

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
