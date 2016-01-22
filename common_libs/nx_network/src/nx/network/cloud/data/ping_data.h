
#pragma once

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

/** [connection_mediator, 4.3.1] */
class NX_NETWORK_API PingRequest
:
    public StunMessageData
{
public:
    std::list<SocketAddress> endpoints;

    PingRequest();
    PingRequest(std::list<SocketAddress> _endpoints);

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API PingResponse
:
    public StunMessageData
{
public:
    std::list<SocketAddress> endpoints;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
