#pragma once

#include "stun_message_data.h"

#include "nx/network/stun/cc/custom_stun.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API UdpHolePunchingSynRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::cc::methods::udpHolePunchingSyn;

    UdpHolePunchingSynRequest();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

class NX_NETWORK_API UdpHolePunchingSynResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::cc::methods::udpHolePunchingSyn;

    nx::String connectSessionId;

    UdpHolePunchingSynResponse();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
