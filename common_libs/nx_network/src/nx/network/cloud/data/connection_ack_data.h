#pragma once

#include "connection_method.h"
#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * [connection_mediator, 4.3.8]
 */
class NX_NETWORK_API ConnectionAckRequest:
    public StunRequestData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::connectionAck;

    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    std::list<SocketAddress> forwardedTcpEndpointList;
    std::list<SocketAddress> udpEndpointList;
    CloudConnectVersion cloudConnectVersion;

    ConnectionAckRequest();
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
