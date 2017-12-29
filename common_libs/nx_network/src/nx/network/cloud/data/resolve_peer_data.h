#pragma once

#include <cstdint>
#include <list>

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>

#include "connection_method.h"
#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ResolvePeerRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolvePeer;

    nx::String hostName;

    ResolvePeerRequest(nx::String _hostName = {});
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ResolvePeerResponse:
    public StunResponseData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolvePeer;

    std::list<network::SocketAddress> endpoints;
    ConnectionMethods connectionMethods;

    ResolvePeerResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
