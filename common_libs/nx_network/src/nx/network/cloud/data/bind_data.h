
#pragma once

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

/** [connection_mediator, 4.3.2] */
class NX_NETWORK_API BindRequest
:
    public StunMessageData
{
public:
    std::list<SocketAddress> publicEndpoints;

    BindRequest();
    BindRequest(std::list<SocketAddress> _publicEndpoints);

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
