#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

class CheckOwnStateRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::checkOwnState;

    CheckOwnStateRequest();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

class CheckOwnStateResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::checkOwnState;

    bool isListening = false;

    CheckOwnStateResponse();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
