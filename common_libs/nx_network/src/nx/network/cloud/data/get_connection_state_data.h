#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API GetConnectionStateRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::getConnectionState;

    GetConnectionStateRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API GetConnectionStateResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::getConnectionState;

    enum State: int { connected = 0, listening = 1 };
    State state = State::connected;

    GetConnectionStateResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
