// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <nx/network/cloud/cloud_connect_version.h>
#include <nx/network/socket_common.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

/**
 * Parameters of a request to connect to a listening peer. With this request, a client app
 * specifies which peer it wants to connect to and which connection methods it supports. This
 * may include UDP hole punching, direct TCP connection, and relaying via Cloud servers.
 */
struct NX_NETWORK_API ConnectRequestData
{
    /**%apidoc The name of the target peer. If peer with exact same name is not found, then all
     * peers with name "smth.{destinationHostName}" are considered for connection.
     */
    std::string destinationHostName;

    /**%apidoc Id of the connecting peer. */
    std::string originatingPeerId;

    /**%apidoc Id of the connect session. It is passed to the target peer and may be used by client
     * to verify the peer after the connection.
     */
    std::string connectSessionId;

    /**%apidoc:integer Connection methods offered by the client.
     * This is a bitmask which can include the following values:
     * - 0x01 UDP hole punching
     * - 0x04 Relaying via Cloud servers
     */
    ConnectionMethods connectionMethods = 0;

    /**%apidoc:stringArray UDP endpoints of the client which should be used by the peer for UDP
     * hole punching.
     */
    std::vector<network::SocketAddress> udpEndpointList;

    /**%apidoc if true, mediator does not report Connect request source address to the server peer.
     * Only addresses found in udpEndpointList are reported.
     */
    bool ignoreSourceAddress = false;

    /**%apidoc:integer Version of the cloud connect implementation the client has.
     * The default value defined in the code must be used.
     */
    CloudConnectVersion cloudConnectVersion = kCurrentCloudConnectVersion;
};

NX_REFLECTION_INSTRUMENT(ConnectRequestData, (destinationHostName)(originatingPeerId) \
    (connectSessionId)(connectionMethods)(udpEndpointList)(ignoreSourceAddress)(cloudConnectVersion))

/**
 * Parameters of a request to connect to a listening peer. With this request, a client app
 * specifies which peer it wants to connect to and which connection methods it supports. This
 * may include UDP hole punching, direct TCP connection, and relaying via Cloud servers.
 */
struct NX_NETWORK_API ConnectRequest:
    ConnectRequestData,
    StunRequestData
{
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    ConnectRequest();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API ConnectResponseData
{
    /**%apidoc:stringArray Public TCP endpoints of the target peer. Client may try to establish
     * regular TCP connection to these endpoints.
     * Note that these may be addresses within peer's LAN so client may connect to some
     * other service within its LAN when using one of these endpoints.
     * So, any successful connection should be verified by means that are out of scope
     * of the Cloud connect.
     */
    std::vector<network::SocketAddress> forwardedTcpEndpointList;

    /**%apidoc:stringArray The list of peer's UDP endpoints to use to do UDP hole punching. */
    std::vector<network::SocketAddress> udpEndpointList;

    /**%apidoc This field has been preserved to keep JSON representation backward-compatible.
     * It contains some element from the trafficRelayUrls array.
     * TODO: Remove after the end of support of 4.2.
     */
    std::optional<std::string> trafficRelayUrl;

    /**%apidoc The list of traffic relay URLs to use to reach the listening peer.
     * Any single of these relays should be able to connect to the listening peer.
     * Multiple URLs are provided just in case if the client cannot reach some relay.
     */
    std::vector<std::string> trafficRelayUrls;

    /**%apidoc Full name of the destination peer.
     * May differ from name found in the ConnectRequest if connect to a system was requested.
     * In this case, name of some peer of the system will be found here.
     */
    std::string destinationHostFullName;

    /**%apidoc Various parameters of the connect process. This mostly includes various timeouts
     * that are placed here to be able to adjust them on the fly on the Cloud side.
     */
    ConnectionParameters params;

    /**%apidoc:integer The version of the cloud connect the peer is using. Client may use it to
     * limit its functionality when connecting to older peers.
     */
    CloudConnectVersion cloudConnectVersion = kCurrentCloudConnectVersion;

    /**%apidoc:string Endpoint of a different mediator node to try. */
    std::optional<network::SocketAddress> alternateMediatorEndpointStunUdp;
};

NX_REFLECTION_INSTRUMENT(ConnectResponseData, (forwardedTcpEndpointList)(udpEndpointList) \
    (trafficRelayUrl)(trafficRelayUrls)(destinationHostFullName)(params)(cloudConnectVersion) \
    (alternateMediatorEndpointStunUdp))

struct NX_NETWORK_API ConnectResponse:
    ConnectResponseData,
    StunResponseData
{
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connect;

    ConnectResponse();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

static constexpr std::string_view kDelayParamName = "delay";

} // namespace nx::hpm::api
