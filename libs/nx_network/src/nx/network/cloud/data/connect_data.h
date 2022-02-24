// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/network/cloud/cloud_connect_version.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

/**
 * [connection_mediator, 4.3.5]
 */
struct NX_NETWORK_API ConnectRequest:
    public StunRequestData
{
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    std::string destinationHostName;
    std::string originatingPeerId;
    std::string connectSessionId;
    ConnectionMethods connectionMethods;
    /** If port is zero then mediator uses source port */
    std::vector<network::SocketAddress> udpEndpointList;
    /** if true, mediator does not report Connect request source address to the server peer.
        Only addresses found in udpEndpointList are reported
    */
    bool ignoreSourceAddress;
    CloudConnectVersion cloudConnectVersion;

    ConnectRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

#define ConnectRequest_Fields \
    (destinationHostName)(originatingPeerId)(connectSessionId)(connectionMethods) \
    (udpEndpointList)(ignoreSourceAddress)(cloudConnectVersion)

QN_FUSION_DECLARE_FUNCTIONS(ConnectRequest, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(ConnectRequest, ConnectRequest_Fields)

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API ConnectResponse:
    public StunResponseData
{
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    /** The list of peer's TCP endpoints if peer has some.
     * Note that these may be addresses within peer's LAN so client may connect to some
     * other service within its LAN when using one of these endpoints.
     * So, any successful connection should be verified by means that are out of scope
     * of the Cloud connect.
     */
    std::vector<network::SocketAddress> forwardedTcpEndpointList;

    /** The list of peer's UDP endpoints to use to do UDP hole punching.*/
    std::vector<network::SocketAddress> udpEndpointList;

    /**%apidoc
     * This field has been preserved to keep JSON representation backward-compatible.
     * It contains some element from the trafficRelayUrls array.
     * TODO: Remove after the end of support of 4.2.
     */
    std::optional<std::string> trafficRelayUrl;

    /**%apidoc The list of traffic relay URLs to use to reach the listening peer.
     * Any single of these relays should be able to connect to the listening peer.
     * Multiple URLs are provided just in case if the client cannot reach some relay.
     */
    std::vector<std::string> trafficRelayUrls;

    /**%apidoc
     * May differ from ConnectRequest::destinationHostName
     * if connect by domain name (e.g., cloud system id) has been requested.
     */
    std::string destinationHostFullName;
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion;

    /**%apidoc
     * Set if connect request failed, but the peer was found at another mediator address.
     */
    std::optional<network::SocketAddress> alternateMediatorEndpointStunUdp;

    ConnectResponse();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

static constexpr std::string_view kDelayParamName = "delay";

#define ConnectResponse_Fields \
    (forwardedTcpEndpointList)(udpEndpointList)(trafficRelayUrl)(trafficRelayUrls) \
    (destinationHostFullName)(params)(cloudConnectVersion)(alternateMediatorEndpointStunUdp)

QN_FUSION_DECLARE_FUNCTIONS(ConnectResponse, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(ConnectResponse, ConnectResponse_Fields)

} // namespace nx::hpm::api
