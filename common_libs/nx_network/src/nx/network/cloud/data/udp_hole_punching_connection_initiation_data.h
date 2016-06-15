/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "stun_message_data.h"

#include "nx/network/stun/cc/custom_stun.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API UdpHolePunchingSynRequest
:
    public StunMessageData
{
public:
    constexpr static const stun::cc::methods::Value kMethod = stun::cc::methods::udpHolePunchingSyn;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API UdpHolePunchingSynResponse
:
    public StunMessageData
{
public:
    constexpr static const stun::cc::methods::Value kMethod = stun::cc::methods::udpHolePunchingSyn;

    nx::String connectSessionId;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
