// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"

namespace nx::hpm::api {

class NX_NETWORK_API GetConnectionStateRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::getConnectionState;

    GetConnectionStateRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API GetConnectionStateResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::getConnectionState;

    enum State: int { connected = 0, listening = 1 };
    State state = State::connected;

    GetConnectionStateResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
