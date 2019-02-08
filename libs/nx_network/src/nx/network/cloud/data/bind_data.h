#pragma once

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * [connection_mediator, 4.3.2]
 */
class NX_NETWORK_API BindRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = network::stun::extension::methods::bind;

    std::list<network::SocketAddress> publicEndpoints;

    BindRequest(std::list<network::SocketAddress> _publicEndpoints = {});
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

} // namespace api
} // namespace hpm
} // namespace nx
