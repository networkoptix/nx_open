#pragma once

#include <list>
#include <optional>

#include <nx/network/cloud/cloud_connect_version.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * [connection_mediator, 4.3.5]
 */
class NX_NETWORK_API ConnectRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    //TODO #ak destinationHostName MUST be unicode string (e.g., QString)
    nx::String destinationHostName;
    nx::String originatingPeerId;
    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    /** If port is zero then mediator uses source port */
    std::list<network::SocketAddress> udpEndpointList;
    /** if true, mediator does not report Connect request source address to the server peer.
        Only addresses found in udpEndpointList are reported
    */
    bool ignoreSourceAddress;
    CloudConnectVersion cloudConnectVersion;

    ConnectRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ConnectResponse:
    public StunResponseData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    std::list<network::SocketAddress> forwardedTcpEndpointList;
    std::list<network::SocketAddress> udpEndpointList;
    /** Optional for backward compatibility. */
    std::optional<nx::String> trafficRelayUrl;
    /**
     * May differ from ConnectRequest::destinationHostName
     * if connect by domain name (e.g., cloud system id) has been requested.
     */
    nx::String destinationHostFullName;
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion;

    ConnectResponse();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
