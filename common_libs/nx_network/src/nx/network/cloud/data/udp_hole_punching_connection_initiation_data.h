/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API UdpHolePunchingSyn
:
    public StunMessageData
{
public:
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API UdpHolePunchingSynAck
:
    public StunMessageData
{
public:
    nx::String connectSessionId;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
