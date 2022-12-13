// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/cloud/cloud_connect_version.h>

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

class NX_NETWORK_API ConnectionRequestedEvent:
    public StunIndicationData
{
public:
    constexpr static const network::stun::extension::indications::Value kMethod =
        network::stun::extension::indications::connectionRequested;

    std::string connectSessionId;
    std::string originatingPeerID;
    /** Peer UDP addresses. */
    std::vector<network::SocketAddress> udpEndpointList;
    std::vector<network::SocketAddress> tcpReverseEndpointList;
    /** All requests connection types. */
    ConnectionMethods connectionMethods = 0;
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion = kCurrentCloudConnectVersion;
    bool isPersistent = false;

    ConnectionRequestedEvent();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
