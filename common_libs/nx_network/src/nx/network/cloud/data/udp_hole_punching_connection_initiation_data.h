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
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod = 
        stun::cc::methods::udpHolePunchingSyn;

    UdpHolePunchingSynRequest();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

class NX_NETWORK_API UdpHolePunchingSynResponse
:
    public StunResponseData
{
public:
    constexpr static const stun::cc::methods::Value kMethod = 
        stun::cc::methods::udpHolePunchingSyn;

    UdpHolePunchingSynResponse();

    nx::String connectSessionId;

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //api
}   //hpm
}   //nx
