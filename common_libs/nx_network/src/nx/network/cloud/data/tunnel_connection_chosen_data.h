/**********************************************************
* Jun 14, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "stun_message_data.h"

#include "nx/network/stun/cc/custom_stun.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API TunnelConnectionChosenRequest
:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::cc::methods::tunnelConnectionChosen;

    TunnelConnectionChosenRequest();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

class NX_NETWORK_API TunnelConnectionChosenResponse
:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::cc::methods::tunnelConnectionChosen;

    TunnelConnectionChosenResponse();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //namespace api
}   //namespace hpm
}   //namespace nx
