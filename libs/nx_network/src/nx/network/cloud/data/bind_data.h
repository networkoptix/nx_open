// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "stun_message_data.h"

namespace nx::hpm::api {

/**
 * [connection_mediator, 4.3.2]
 */
class NX_NETWORK_API BindRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::bind;

    std::vector<network::SocketAddress> publicEndpoints;

    BindRequest(std::vector<network::SocketAddress> _publicEndpoints = {});
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API BindResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::bind;

    BindResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
