// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/stun/extension/stun_extension_types.h>

#include "stun_message_data.h"

namespace nx::hpm::api {

class NX_NETWORK_API UdpHolePunchingSynRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::udpHolePunchingSyn;

    UdpHolePunchingSynRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API UdpHolePunchingSynResponse:
    public StunResponseData
{
public:
    static constexpr auto kMethod = network::stun::extension::methods::udpHolePunchingSyn;

    std::string connectSessionId;

    UdpHolePunchingSynResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
