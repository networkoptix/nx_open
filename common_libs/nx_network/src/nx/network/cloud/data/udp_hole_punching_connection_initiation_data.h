#pragma once

#include "stun_message_data.h"

#include "nx/network/stun/extension/stun_extension_types.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API UdpHolePunchingSynRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::udpHolePunchingSyn;

    UdpHolePunchingSynRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API UdpHolePunchingSynResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::udpHolePunchingSyn;

    nx::String connectSessionId;

    UdpHolePunchingSynResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
