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
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connectionAck;

    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    std::list<network::SocketAddress> forwardedTcpEndpointList;
    std::list<network::SocketAddress> udpEndpointList;
    CloudConnectVersion cloudConnectVersion;

    ConnectionAckRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
