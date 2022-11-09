// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/network/abstract_socket.h>
#include <nx/network/cloud/cloud_connect_version.h>
#include <nx/network/cloud/cloud_connect_options.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"

namespace nx::hpm::api {

struct NX_NETWORK_API ListenRequest:
    StunRequestData
{
    static constexpr auto kMethod = network::stun::extension::methods::listen;

    // TODO: #muskov Remove systemId and serverId as redundant.
    // Every server message is signed up with system id, server id and message integrity based on
    // authentication key by MediatorServerConnection.
    std::string systemId;
    std::string serverId;
    CloudConnectVersion cloudConnectVersion = kCurrentCloudConnectVersion;

    ListenRequest();

    void serializeAttributes(nx::network::stun::Message* const message);
    bool parseAttributes(const nx::network::stun::Message& message);
};

struct NX_NETWORK_API ListenResponse:
    StunResponseData
{
    static constexpr auto kMethod = network::stun::extension::methods::listen;
    static constexpr auto kDefaultRelayConnectTimeout = std::chrono::seconds(21);

    std::optional<network::KeepAliveOptions> tcpConnectionKeepAlive;
    CloudConnectOptions cloudConnectOptions;

    /**
     * This field left for compatibility between internal 3.1 builds.
     * TODO: #akolesnikov Remove in 3.2.
     */
    std::optional<std::string> trafficRelayUrl;
    std::vector<std::string> trafficRelayUrls;
    std::chrono::milliseconds trafficRelayConnectTimeout = kDefaultRelayConnectTimeout;

    ListenResponse();

    void serializeAttributes(nx::network::stun::Message* const message);
    bool parseAttributes(const nx::network::stun::Message& message);
};

} // namespace nx::hpm::api
