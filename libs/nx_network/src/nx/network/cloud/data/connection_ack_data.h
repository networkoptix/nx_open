// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/network/cloud/cloud_connect_version.h>

#include "connection_method.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

/**
 * [connection_mediator, 4.3.8]
 */
class NX_NETWORK_API ConnectionAckRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::connectionAck;

    std::string connectSessionId;
    ConnectionMethods connectionMethods = 0;
    std::vector<network::SocketAddress> forwardedTcpEndpointList;
    std::vector<network::SocketAddress> udpEndpointList;
    CloudConnectVersion cloudConnectVersion = kCurrentCloudConnectVersion;

    ConnectionAckRequest();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
