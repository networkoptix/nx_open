#pragma once

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * [connection_mediator, 4.3.1]
 */
class NX_NETWORK_API PingRequest:
    public StunRequestData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::ping;

    std::list<SocketAddress> endpoints;

    PingRequest();
    PingRequest(std::list<SocketAddress> _endpoints);

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API PingResponse:
    public StunResponseData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::ping;

    std::list<SocketAddress> endpoints;

    PingResponse();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
